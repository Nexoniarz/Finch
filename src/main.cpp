#include "cimport.h"
#include "codegen.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

#include "rt_source.inc"  // kRuntimeSource: runtime/finch_rt.c, embedded at build time

using namespace llvm;

static void usage() {
    std::fprintf(stderr,
                 "Finch " FINCH_VERSION " - small, quick, sharp.\n\n"
                 "usage:\n"
                 "  finch run   <file.fch> [args...]    compile and run\n"
                 "  finch build <file.fch> [-o name]    compile to a program\n"
                 "  finch ir    <file.fch>              show the generated LLVM IR\n"
                 "  finch version                      show the version\n"
                 "\n"
                 "  -l <lib>   link a C library, same as  link \"lib\"  in the file\n"
                 "  -g         add debug info (for gdb / lldb)\n"
                 "  -O0        skip optimizations (to read the raw IR)\n");
    std::exit(1);
}

static bool readFile(const std::string &path, std::string &out) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

static std::string dirOf(const std::string &path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

// ---------- loading the program and its modules ----------

struct Loader {
    std::vector<Program> progs;
    std::map<std::string, bool> loaded;  // module name -> seen

    void load(const std::string &path, const std::string &module, Pos from) {
        std::string text;
        if (!readFile(path, text)) {
            if (from.line) failAt(from.file, from.line, from.col, "can't find the module '" + module + "' (looked for " + path + ")");
            std::fprintf(stderr, "error: can't open '%s'\n", path.c_str());
            std::exit(1);
        }
        int idx = (int)g_files.size();
        g_files.push_back({path, text});
        Program p = parse(lex(idx), idx);
        p.module = module;
        p.path = path;
        std::vector<Import> mods;
        for (const Import &im : p.imports)
            if (!im.isC) mods.push_back(im);
        progs.push_back(std::move(p));
        for (const Import &im : mods) {
            if (im.path == "main") failAt(im.pos.file, im.pos.line, im.pos.col, "'main' can't be imported");
            if (loaded[im.path]) continue;
            loaded[im.path] = true;
            load(findModule(dirOf(path), im.path), im.path, im.pos);
        }
    }

    // next to the importing file, then in $FINCH_PATH folders
    static std::string findModule(const std::string &dir, const std::string &name) {
        std::string here = dir + "/" + name + ".fch";
        if (sys::fs::exists(here)) return here;
        if (const char *fp = std::getenv("FINCH_PATH")) {
            std::stringstream ss(fp);
            for (std::string d; std::getline(ss, d, ':');)
                if (!d.empty() && sys::fs::exists(d + "/" + name + ".fch")) return d + "/" + name + ".fch";
        }
        return here;
    }
};

// ---------- LLVM ----------

static void optimize(Module &mod, TargetMachine *tm) {
    LoopAnalysisManager lam;
    FunctionAnalysisManager fam;
    CGSCCAnalysisManager cgam;
    ModuleAnalysisManager mam;
    PassBuilder pb(tm);
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    pb.buildPerModuleDefaultPipeline(OptimizationLevel::O2).run(mod, mam);
}

static TargetMachine *hostMachine() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    Triple triple(sys::getDefaultTargetTriple());
    std::string err;
    const Target *target = TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        std::exit(1);
    }
    return target->createTargetMachine(triple, "generic", "", TargetOptions(), Reloc::PIC_);
}

static void emitObject(Module &mod, TargetMachine *tm, const std::string &path) {
    std::error_code ec;
    raw_fd_ostream out(path, ec, sys::fs::OF_None);
    if (ec) {
        std::fprintf(stderr, "error: can't write '%s': %s\n", path.c_str(), ec.message().c_str());
        std::exit(1);
    }
    legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::ObjectFile)) {
        std::fprintf(stderr, "error: this machine can't produce object files\n");
        std::exit(1);
    }
    pm.run(mod);
}

// ---------- linking ----------

// Run a shell command; returns its exit status, everything it printed goes to `output`.
static int capture(const std::string &cmd, std::string &output) {
    FILE *p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) output.append(buf, n);
    int status = pclose(p);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static std::string ccName() {
    const char *cc = std::getenv("CC");
    return cc ? cc : "cc";
}

// The runtime is compiled once per version and kept in ~/.cache/finch.
static std::string runtimeObject() {
    std::string dir;
    if (const char *x = std::getenv("XDG_CACHE_HOME")) dir = std::string(x) + "/finch";
    else if (const char *h = std::getenv("HOME")) dir = std::string(h) + "/.cache/finch";
    else dir = "/tmp/finch-cache";
    size_t hash = std::hash<std::string>()(std::string(kRuntimeSource) + FINCH_VERSION + ccName());
    std::string obj = dir + "/rt-" + std::to_string(hash) + ".o";
    if (sys::fs::exists(obj)) return obj;

    sys::fs::create_directories(dir);
    std::string src = obj + ".c", part = obj + ".part";
    {
        std::ofstream out(src);
        out << kRuntimeSource;
    }
    std::string output;
    int status = capture(ccName() + " -O2 -fPIC -c '" + src + "' -o '" + part + "'", output);
    std::remove(src.c_str());
    if (status != 0) {
        std::fprintf(stderr, "%serror: couldn't build the Finch runtime (is a C compiler installed?)\n", output.c_str());
        std::exit(1);
    }
    sys::fs::rename(part, obj);
    return obj;
}

// pkg-config knows the right paths for a library if it has one; otherwise plain -l<name>.
static std::string libFlags(const std::string &lib) {
    std::string out;
    if (capture("pkg-config --libs '" + lib + "'", out) == 0) {
        while (!out.empty() && std::isspace((unsigned char)out.back())) out.pop_back();
        return " " + out;
    }
    return " '-l" + lib + "'";
}

// "GLFW/glfw3.h" -> "glfw": a decent first guess for the library's name.
static std::string guessLib(const std::string &header) {
    std::string name = header.substr(header.find_last_of('/') + 1);
    name = name.substr(0, name.rfind('.'));
    while (!name.empty() && std::isdigit((unsigned char)name.back())) name.pop_back();
    for (char &ch : name) ch = std::tolower((unsigned char)ch);
    return name;
}

// The linker speaks in riddles; turn its complaints into something you can act on.
[[noreturn]] static void linkFailed(const std::string &output, const CImports &c) {
    std::vector<std::string> missing, missingLibs;
    auto addOnce = [](std::vector<std::string> &v, const std::string &x) {
        if (std::find(v.begin(), v.end(), x) == v.end()) v.push_back(x);
    };
    std::istringstream lines(output);
    for (std::string l; std::getline(lines, l);) {
        size_t a;
        if ((a = l.find("undefined reference to `")) != std::string::npos) {
            a += 24;
            addOnce(missing, l.substr(a, l.find('\'', a) - a));
        } else if ((a = l.find("undefined symbol: ")) != std::string::npos) {
            a += 18;
            addOnce(missing, l.substr(a, l.find_first_of(" \n", a) - a));
        } else if ((a = l.find("cannot find -l")) != std::string::npos) {
            a += 14;
            addOnce(missingLibs, l.substr(a, l.find_first_of(": \n", a) - a));
        }
    }
    if (missing.empty() && missingLibs.empty()) {
        std::fprintf(stderr, "%serror: linking failed\n", output.c_str());
        std::exit(1);
    }

    for (const std::string &lib : missingLibs)
        std::fprintf(stderr, "error: the library '%s' wasn't found. Is it installed? (on Nix: add it to shell.nix)\n", lib.c_str());

    std::vector<std::pair<std::string, std::vector<std::string>>> byHeader;
    for (const std::string &name : missing) {
        auto f = c.fns.find(name);
        std::string header = f == c.fns.end() ? "" : f->second.header;
        auto it = std::find_if(byHeader.begin(), byHeader.end(), [&](auto &g) { return g.first == header; });
        if (it == byHeader.end()) byHeader.push_back({header, {name}});
        else it->second.push_back(name);
    }
    for (auto &[header, names] : byHeader) {
        std::string list;
        for (size_t k = 0; k < names.size() && k < 3; k++) list += (k ? ", " : "") + names[k];
        if (names.size() > 3) list += " and " + std::to_string(names.size() - 3) + " more";
        if (header.empty()) {
            std::fprintf(stderr, "error: no code found for %s\n", list.c_str());
            continue;
        }
        std::fprintf(stderr,
                     "error: %s come%s from \"%s\", but its library isn't linked\n"
                     "  the header only says the functions exist; their code lives in a library.\n"
                     "  add this at the top of your file (with the library's real name):\n\n"
                     "      link \"%s\"\n\n",
                     list.c_str(), names.size() == 1 ? "s" : "", header.c_str(), guessLib(header).c_str());
    }
    std::exit(1);
}

static bool endsWith(const std::string &s, const char *x) {
    size_t n = std::strlen(x);
    return s.size() > n && s.compare(s.size() - n, n, x) == 0;
}

static void link(const std::string &obj, const std::string &exe, const std::vector<std::string> &libs,
                 const CImports &c) {
    std::string cmd = ccName() + " '" + obj + "' '" + runtimeObject() + "' -o '" + exe + "' -lm";
    std::vector<std::string> temps;
    for (const std::string &l : libs) {
        if (endsWith(l, ".c")) {  // compile the user's C file
            std::string o = obj + "." + std::to_string(temps.size()) + ".o", output;
            if (capture(ccName() + " -O2 -fPIC -c '" + l + "' -o '" + o + "'", output) != 0) {
                std::fprintf(stderr, "%serror: couldn't compile %s\n", output.c_str(), l.c_str());
                std::exit(1);
            }
            temps.push_back(o);
            cmd += " '" + o + "'";
        } else if (endsWith(l, ".o") || endsWith(l, ".a")) {
            cmd += " '" + l + "'";
        } else {
            cmd += libFlags(l);
        }
    }
    std::string output;
    int status = capture(cmd, output);
    std::remove(obj.c_str());
    for (const std::string &t : temps) std::remove(t.c_str());
    if (status == 127) {
        std::fprintf(stderr, "error: no C compiler found to link with (install gcc or clang, or set CC)\n");
        std::exit(1);
    }
    if (status != 0) linkFailed(output, c);
    std::fputs(output.c_str(), stderr);  // warnings, if any
}

static std::string stem(const std::string &path) {
    size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

static std::string shellQuote(const std::string &s) {
    std::string out = "'";
    for (char ch : s) out += ch == '\'' ? std::string("'\\''") : std::string(1, ch);
    return out + "'";
}

int main(int argc, char **argv) {
    if (argc == 2 && (std::string(argv[1]) == "version" || std::string(argv[1]) == "--version")) {
        std::printf("finch %s (LLVM %s)\n", FINCH_VERSION, LLVM_VERSION_STRING);
        return 0;
    }
    if (argc < 3) usage();
    std::string cmd = argv[1], file = argv[2];
    if (cmd != "run" && cmd != "build" && cmd != "ir") usage();
    std::string out = stem(file);
    std::vector<std::string> libs, programArgs;
    bool optimizeCode = true, debug = false;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (cmd == "run" && !programArgs.empty()) programArgs.push_back(a);
        else if (a == "-o" && i + 1 < argc && cmd == "build") out = argv[++i];
        else if (a == "-O0") optimizeCode = false;
        else if (a == "-g") debug = true;
        else if (a == "-l" && i + 1 < argc) libs.push_back(argv[++i]);
        else if (a.rfind("-l", 0) == 0 && a.size() > 2) libs.push_back(a.substr(2));
        else if (cmd == "run") programArgs.push_back(a);
        else usage();
    }

    Loader loader;
    loader.load(file, "", Pos{});
    std::vector<Import> allImports;
    std::vector<std::string> dirs;
    for (const Program &p : loader.progs) {
        allImports.insert(allImports.end(), p.imports.begin(), p.imports.end());
        for (const Link &l : p.links) {
            bool file = endsWith(l.lib, ".c") || endsWith(l.lib, ".o") || endsWith(l.lib, ".a");
            std::string path = file && l.lib[0] != '/' ? dirOf(p.path) + "/" + l.lib : l.lib;
            if (file && !sys::fs::exists(path)) failAt(l.pos.file, l.pos.line, l.pos.col, "can't find the file '" + path + "'");
            libs.push_back(path);
        }
        if (std::find(dirs.begin(), dirs.end(), dirOf(p.path)) == dirs.end()) dirs.push_back(dirOf(p.path));
    }
    CImports c = importHeaders(allImports, dirs);
    LLVMContext ctx;
    TargetMachine *tm = hostMachine();
    std::unique_ptr<Module> mod = generate(loader.progs, c, ctx, *tm, debug);
    if (optimizeCode) optimize(*mod, tm);

    if (cmd == "ir") {
        mod->print(outs(), nullptr);
        return 0;
    }

    std::string obj = out + ".o";
    if (cmd == "run") {
        SmallString<128> tmp;
        sys::fs::createTemporaryFile("finch", "", tmp);
        out = tmp.str().str();
        obj = out + ".o";
    }
    emitObject(*mod, tm, obj);
    link(obj, out, libs, c);

    if (cmd == "run") {
        std::string line = shellQuote(out);
        for (const std::string &a : programArgs) line += " " + shellQuote(a);
        int status = std::system(line.c_str());
        std::remove(out.c_str());
        if (WIFSIGNALED(status)) {
            std::fprintf(stderr, "\nthe program crashed: %s\n", strsignal(WTERMSIG(status)));
            return 128 + WTERMSIG(status);
        }
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return 0;
}
