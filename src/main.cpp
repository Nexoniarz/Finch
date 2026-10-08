#include "cimport.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <string>
#include <vector>

using namespace llvm;

static void usage() {
    std::fprintf(stderr,
                 "Finch " FINCH_VERSION " - small, quick, sharp.\n\n"
                 "usage:\n"
                 "  finch run   <file.fn>              compile and run\n"
                 "  finch build <file.fn> [-o name]    compile to a program\n"
                 "\n"
                 "  -l <lib>   link a C library, same as  link \"lib\"  in the file\n"
                 "  -O0        skip optimizations (to read the raw IR)\n"
                 "  finch ir    <file.fn>              show the generated LLVM IR\n"
                 "  finch version                      show the version\n");
    std::exit(1);
}

static std::string readFile(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "error: can't open '%s'\n", path.c_str());
        std::exit(1);
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

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

    // group the missing functions by the header they came from
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

static void link(const std::string &obj, const std::string &exe, const std::vector<std::string> &libs,
                 const CImports &c) {
    const char *cc = std::getenv("CC");
    std::string cmd = std::string(cc ? cc : "cc") + " '" + obj + "' -o '" + exe + "' -lm";
    for (const std::string &l : libs) cmd += libFlags(l);
    std::string output;
    int status = capture(cmd, output);
    std::remove(obj.c_str());
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

int main(int argc, char **argv) {
    if (argc == 2 && (std::string(argv[1]) == "version" || std::string(argv[1]) == "--version")) {
        std::printf("finch %s (LLVM %s)\n", FINCH_VERSION, LLVM_VERSION_STRING);
        return 0;
    }
    if (argc < 3) usage();
    std::string cmd = argv[1], file = argv[2];
    std::string out = stem(file);
    std::vector<std::string> libs;
    bool optimizeCode = true;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) out = argv[++i];
        else if (a == "-O0") optimizeCode = false;
        else if (a == "-l" && i + 1 < argc) libs.push_back(argv[++i]);
        else if (a.rfind("-l", 0) == 0 && a.size() > 2) libs.push_back(a.substr(2));
        else usage();
    }
    if (cmd != "run" && cmd != "build" && cmd != "ir") usage();

    Program prog = parse(lex(readFile(file), file));
    for (const Link &l : prog.links) libs.push_back(l.lib);
    size_t slash = file.find_last_of('/');
    CImports c = importHeaders(prog.imports, slash == std::string::npos ? "." : file.substr(0, slash));
    LLVMContext ctx;
    TargetMachine *tm = hostMachine();
    std::unique_ptr<Module> mod = generate(prog, c, ctx, *tm);
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
        int status = std::system(("'" + out + "'").c_str());
        std::remove(out.c_str());
        if (WIFSIGNALED(status)) {
            std::fprintf(stderr, "\nthe program crashed: %s\n", strsignal(WTERMSIG(status)));
            return 128 + WTERMSIG(status);
        }
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return 0;
}
