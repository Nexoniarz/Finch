// The Finch runtime: string and array helpers, input, files and runtime errors.
// It is embedded in the compiler, compiled once into a cache folder, and linked
// statically into every Finch program. Everything here is plain C with no state.
//
// Layout shared with the compiler (src/gen.fch):
//   str  = { char *ptr; int64 len; int64 cap }   ptr is always NUL-terminated
//          cap > 0  : heap memory owned by the value (cap bytes allocated)
//          cap == 0 : a string literal, valid forever, never written to
//          cap == -1: borrowed from C, must be copied before it is kept
//   []T  = { T *ptr;   int64 len; int64 cap }    owns ptr when cap > 0
//   map[K]V: see "maps" below

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

typedef struct {
    char *ptr;
    int64_t len;
    int64_t cap;
} FStr;

typedef struct {
    void *ptr;
    int64_t len;
    int64_t cap;
} FArr;

static char empty[1] = "";

// ---------- runtime errors ----------

// A program that loads Finch libraries (finch build --shared) can take runtime errors over
// instead of the process exiting: finch_module_on_fatal(handler) in each library. The handler
// gets the message and must not return (longjmp out, or exit itself).
static void (*fatal_hook)(const char *msg);
void finch_on_fatal(void (*hook)(const char *msg)) { fatal_hook = hook; }

// The chain of Finch calls, kept by the program itself (see @finch_trace in the compiler's output):
// each frame is { the caller's frame, the function's name, its file, the line it is at }.
typedef struct FFrame {
    struct FFrame *prev;
    const char *name;
    const char *file;
    int64_t line;
} FFrame;
static void *(*trace_top)(void);
static void (*trace_set)(void *);
void finch_trace_init(void *(*top)(void), void (*set)(void *)) {
    trace_top = top;
    trace_set = set;
}

// Colours, only for a terminal a person looks at (not a pipe or a file): NO_COLOR turns them off,
// FINCH_COLOR=always|never decides. fd 1 is the output, 2 the errors.
static int color_on(int fd) {
    static int known[3] = {-1, -1, -1};
    if (known[fd] >= 0) return known[fd];
    const char *force = getenv("FINCH_COLOR");
    const char *term = getenv("TERM");
    int on;
    if (force && strcmp(force, "always") == 0) on = 1;
    else if (force && strcmp(force, "never") == 0) on = 0;
    else if (getenv("NO_COLOR") && *getenv("NO_COLOR")) on = 0;
    else if (term && strcmp(term, "dumb") == 0) on = 0;
#ifdef _WIN32
    else if (!_isatty(fd)) on = 0;
    else {
        HANDLE h = GetStdHandle(fd == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);     // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        on = 1;
    }
#else
    else on = isatty(fd);
#endif
    known[fd] = on;
    return on;
}
#define PAINT(fd, code) (color_on(fd) ? "\033[" code "m" : "")
#define PLAIN(fd) (color_on(fd) ? "\033[0m" : "")

// line `line` of a source file, for showing where a program stopped (a terminal only)
static void show_source_line(const char *file, int64_t line) {
    FILE *f = fopen(file, "r");
    if (!f) return;
    char buf[512];
    int64_t n = 0;
    while (fgets(buf, sizeof buf, f)) {
        if (strchr(buf, '\n') || feof(f)) n++;
        if (n == line) {
            size_t len = strlen(buf);
            while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = 0;
            fprintf(stderr, " %s%lld |%s %s\n", PAINT(2, "1;34"), (long long)line, PLAIN(2), buf);
            break;
        }
    }
    fclose(f);
}

static void print_trace(void) {
    if (!trace_top) return;
    FFrame *f = trace_top();
    for (int n = 0; f; n++, f = f->prev) {
        if (n == 30) {
            fprintf(stderr, "  ...\n");
            break;
        }
        fprintf(stderr, "%s  in %s (%s:%lld)%s\n", PAINT(2, "2"), f->name, f->file, (long long)f->line, PLAIN(2));
    }
}

static const char *stop_file;
static int64_t stop_line;

static __attribute__((noreturn)) void fatal(const char *msg) {
    if (fatal_hook) fatal_hook(msg);
    fflush(stdout);
    const char *mark = strstr(msg, "runtime error:");
    if (mark && color_on(2)) {
        // "file:line: runtime error: what" -> the place in bold, the words in red, then the line of code itself
        fprintf(stderr, "%s%.*s%s%sruntime error:%s%s%s%s\n", PAINT(2, "1"), (int)(mark - msg), msg, PLAIN(2),
                PAINT(2, "1;31"), PLAIN(2), PAINT(2, "1"), mark + 14, PLAIN(2));
        if (stop_file) show_source_line(stop_file, stop_line);
    } else {
        fprintf(stderr, "%s\n", msg);
    }
    print_trace();
    exit(1);
}

__attribute__((noreturn, cold)) void finch_panic(const char *file, int64_t line, const char *msg) {
    char full[1024];
    snprintf(full, sizeof full, "%s:%lld: runtime error: %s", file, (long long)line, msg);
    fflush(stdout);
    stop_file = file;
    stop_line = line;
    fatal(full);
}

__attribute__((noreturn, cold)) void finch_panic_index(const char *file, int64_t line, int64_t i, int64_t len) {
    char msg[128];
    snprintf(msg, sizeof msg, "index %lld is out of range (the length is %lld)", (long long)i, (long long)len);
    finch_panic(file, line, msg);
}

static void *must(void *p) {
    if (!p) {
        fflush(stdout);
        fatal("runtime error: out of memory");
    }
    return p;
}

void *finch_alloc(int64_t n) { return must(malloc(n ? (size_t)n : 1)); }
void *finch_zalloc(int64_t n) { return must(calloc(1, n ? (size_t)n : 1)); }

// ---------- functions as values ----------
// A function value is { function, environment }. An environment (the copies a function written in the code took of
// the variables around it) starts with two helpers made by the compiler: how to drop it and how to copy it.

typedef struct {
    void (*drop)(void *env);
    void *(*copy)(void *env);
} FEnvHead;

typedef struct {
    void *fn;
    void *env;
} FFn;

void finch_fn_copy(FFn *dst, const FFn *src) {
    void *env = src->env ? ((FEnvHead *)src->env)->copy(src->env) : NULL;
    dst->fn = src->fn;
    dst->env = env;
}

void finch_fn_drop(FFn *f) {
    if (f->env) ((FEnvHead *)f->env)->drop(f->env);
    f->env = NULL;
}

// An interface value is { box, table }: the table starts with how to drop, copy and format what is in the box.
typedef struct {
    void *data;
    void **table;
} FIface;

void finch_iface_copy(FIface *dst, const FIface *src) {
    void *data = src->data ? ((void *(*)(void *))src->table[1])(src->data) : NULL;
    dst->data = data;
    dst->table = src->table;
}

void finch_iface_drop(FIface *v) {
    if (v->data) ((void (*)(void *))v->table[0])(v->data);
    v->data = NULL;
}

// list.sort_by(less): a stable merge sort; less(ctx, a, b) says whether the element at a goes before the one at b.
// Elements are moved as bytes (nothing is copied or dropped), so it works for every element type.
void finch_sort_by(void *data, int64_t n, int64_t size, int (*less)(void *, const void *, const void *), void *ctx) {
    if (n < 2) return;
    char *a = data;
    char *tmp = finch_alloc(n * size);
    for (int64_t width = 1; width < n; width *= 2) {
        for (int64_t lo = 0; lo < n; lo += 2 * width) {
            int64_t mid = lo + width < n ? lo + width : n;
            int64_t hi = lo + 2 * width < n ? lo + 2 * width : n;
            int64_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                if (less(ctx, a + j * size, a + i * size)) {        // the right one only if it is strictly less: stable
                    memcpy(tmp + k * size, a + j * size, size);
                    j++;
                } else {
                    memcpy(tmp + k * size, a + i * size, size);
                    i++;
                }
                k++;
            }
            memcpy(tmp + k * size, a + i * size, (mid - i) * size);
            k += mid - i;
            memcpy(tmp + k * size, a + j * size, (hi - j) * size);
        }
        memcpy(a, tmp, n * size);
    }
    free(tmp);
}

// ---------- assert and finch test ----------

__attribute__((noreturn, cold)) void finch_assert_failed(const char *file, int64_t line, const FStr *why) {
    char msg[600];
    if (why->len) snprintf(msg, sizeof msg, "assert failed: %.*s", (int)(why->len > 500 ? 500 : why->len), why->ptr);
    else snprintf(msg, sizeof msg, "assert failed");
    finch_panic(file, line, msg);
}

// Each test runs on its own: a runtime error (a failed assert too) ends that test, and the next one runs.
static jmp_buf test_jump;
static char test_message[1100];
static int64_t tests_passed, tests_failed;

static void test_fatal(const char *msg) {
    snprintf(test_message, sizeof test_message, "%s", msg);
    longjmp(test_jump, 1);
}

void finch_test_run(const char *name, void (*fn)(void)) {
    void (*saved)(const char *) = fatal_hook;
    void *chain = trace_top ? trace_top() : NULL;
    test_message[0] = 0;
    fatal_hook = test_fatal;
    int failed = setjmp(test_jump);
    if (!failed) fn();
    fatal_hook = saved;
    if (trace_set) trace_set(chain);        // a test that stopped early left its calls on the chain
    fflush(stdout);
    if (failed) {
        tests_failed++;
        printf("%sFAIL%s %s\n     %s%s%s\n", PAINT(1, "1;31"), PLAIN(1), name, PAINT(1, "2"), test_message, PLAIN(1));
    } else {
        tests_passed++;
        printf("%sok%s   %s\n", PAINT(1, "1;32"), PLAIN(1), name);
    }
    fflush(stdout);
}

int finch_test_summary(void) {
    printf("\n%s%lld passed%s, %s%lld failed%s\n", (tests_passed ? PAINT(1, "1;32") : PAINT(1, "1")), (long long)tests_passed, PLAIN(1),
           (tests_failed ? PAINT(1, "1;31") : PAINT(1, "1")), (long long)tests_failed, PLAIN(1));
    if (color_on(1) && tests_failed == 0 && tests_passed > 0) printf("%sAll green. The finch approves.%s\n", PAINT(1, "2"), PLAIN(1));
    return tests_failed ? 1 : 0;
}

// ---------- strings ----------

static void str_heap(FStr *out, const char *p, int64_t len) {
    char *m = finch_alloc(len + 1);
    if (len) memcpy(m, p, (size_t)len);
    m[len] = 0;
    out->ptr = m;
    out->len = len;
    out->cap = len + 1;
}

static void str_static(FStr *out, const char *lit) {
    out->ptr = (char *)lit;
    out->len = (int64_t)strlen(lit);
    out->cap = 0;
}

// A copy that can be kept: literals are shared, everything else is duplicated.
void finch_str_copy(FStr *out, const FStr *s) {
    if (s->cap == 0) *out = *s;
    else str_heap(out, s->ptr, s->len);
}

// Make `s` writable (before s[i] = c).
void finch_str_own(FStr *s) {
    if (s->cap > 0) return;
    FStr t;
    str_heap(&t, s->ptr, s->len);
    *s = t;
}

void finch_str_drop(FStr *s) {
    if (s->cap > 0) free(s->ptr);
}

void finch_str_from_c(FStr *out, const char *p) {
    if (!p) {
        str_static(out, empty);
        return;
    }
    out->ptr = (char *)p;
    out->len = (int64_t)strlen(p);
    out->cap = -1;
}

void finch_str_concat(FStr *out, const FStr *a, const FStr *b) {
    int64_t len = a->len + b->len;
    char *m = finch_alloc(len + 1);
    memcpy(m, a->ptr, (size_t)a->len);
    memcpy(m + a->len, b->ptr, (size_t)b->len);
    m[len] = 0;
    out->ptr = m;
    out->len = len;
    out->cap = len + 1;
}

int finch_str_eq(const FStr *a, const FStr *b) {
    return a->len == b->len && memcmp(a->ptr, b->ptr, (size_t)a->len) == 0;
}

int64_t finch_str_cmp(const FStr *a, const FStr *b) {
    int64_t n = a->len < b->len ? a->len : b->len;
    int c = memcmp(a->ptr, b->ptr, (size_t)n);
    if (c) return c < 0 ? -1 : 1;
    return a->len < b->len ? -1 : a->len > b->len;
}

void finch_str_from_int(FStr *out, int64_t v) {
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%lld", (long long)v);
    str_heap(out, buf, n);
}

void finch_str_from_uint(FStr *out, uint64_t v) {
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%llu", (unsigned long long)v);
    str_heap(out, buf, n);
}

// The shortest text that reads back as the same number (as an f32 when `single`):
// 0.1 -> "0.1", 1.0/3 -> "0.3333333333333333", 1e8 -> "100000000", 1e20 -> "1e+20".
static int fmt_float(char *buf, size_t size, double v, int single) {
    if (isnan(v)) return snprintf(buf, size, "nan");
    if (isinf(v)) return snprintf(buf, size, v < 0 ? "-inf" : "inf");
    int p = 1;
    for (; p < 17; p++) {
        snprintf(buf, size, "%.*e", p - 1, v);
        double back = strtod(buf, NULL);
        if (single ? (float)back == (float)v : back == v) break;
    }
    int exp = atoi(strchr(buf, 'e') + 1);
    if (exp < -5 || exp >= 17) return snprintf(buf, size, "%.*g", p, v);
    int decimals = p - 1 - exp;
    return snprintf(buf, size, "%.*f", decimals > 0 ? decimals : 0, v);
}

void finch_str_from_float(FStr *out, double v) {
    char buf[64];
    int n = fmt_float(buf, sizeof buf, v, 0);
    str_heap(out, buf, n);
}

void finch_str_from_f32(FStr *out, double v) {
    char buf[64];
    int n = fmt_float(buf, sizeof buf, v, 1);
    str_heap(out, buf, n);
}

// str(x, 2): a fixed number of digits after the point
void finch_str_fixed(FStr *out, double v, int64_t digits) {
    char buf[400];
    if (digits < 0) digits = 0;
    if (digits > 60) digits = 60;
    int n = snprintf(buf, sizeof buf, "%.*f", (int)digits, v);
    str_heap(out, buf, n);
}

void finch_print_float(double v, int32_t single) {
    char buf[64];
    fmt_float(buf, sizeof buf, v, single);
    fputs(buf, stdout);
}

void finch_str_from_char(FStr *out, char c) { str_heap(out, &c, 1); }

void finch_str_from_bool(FStr *out, int b) { str_static(out, b ? "true" : "false"); }

static void number_error(char *msg, size_t size, const FStr *s, const char *kind) {
    snprintf(msg, size, "can't turn \"%.*s\" into %s", (int)(s->len > 100 ? 100 : s->len), s->ptr ? s->ptr : "", kind);
}

static int parse_int(const FStr *s, int64_t *out) {
    const char *p = s->ptr ? s->ptr : "";
    while (isspace((unsigned char)*p)) p++;
    char *end;
    errno = 0;
    long long v = strtoll(p, &end, 10);
    while (isspace((unsigned char)*end)) end++;
    *out = v;
    return !(end == p || *end || errno == ERANGE);
}

static int parse_float(const FStr *s, double *out) {
    const char *p = s->ptr ? s->ptr : "";
    while (isspace((unsigned char)*p)) p++;
    char *end;
    double v = strtod(p, &end);
    while (isspace((unsigned char)*end)) end++;
    *out = v;
    return !(end == p || *end);
}

int64_t finch_str_to_int(const FStr *s, const char *file, int64_t line) {
    int64_t v;
    if (!parse_int(s, &v)) {
        char msg[200];
        number_error(msg, sizeof msg, s, "int");
        finch_panic(file, line, msg);
    }
    return v;
}

double finch_str_to_float(const FStr *s, const char *file, int64_t line) {
    double v;
    if (!parse_float(s, &v)) {
        char msg[200];
        number_error(msg, sizeof msg, s, "float");
        finch_panic(file, line, msg);
    }
    return v;
}

// ---------- errors as values: int(s) or 0, try read_file(p) ... ----------
// Each gives back 1 on success; on failure 0, with the message in *err.

static void set_error(FStr *err, const char *msg) { str_heap(err, msg, (int64_t)strlen(msg)); }

int finch_str_try_int(const FStr *s, int64_t *out, FStr *err) {
    if (parse_int(s, out)) return 1;
    char msg[200];
    number_error(msg, sizeof msg, s, "int");
    set_error(err, msg);
    *out = 0;
    return 0;
}

int finch_str_try_float(const FStr *s, double *out, FStr *err) {
    if (parse_float(s, out)) return 1;
    char msg[200];
    number_error(msg, sizeof msg, s, "float");
    set_error(err, msg);
    *out = 0;
    return 0;
}

static void file_error(FStr *err, const char *what, const FStr *path) {
    char msg[300];
    snprintf(msg, sizeof msg, "can't %s the file \"%.*s\": %s", what, (int)(path->len > 200 ? 200 : path->len),
             path->ptr ? path->ptr : "", strerror(errno));
    set_error(err, msg);
}

// when `fn main() -> !` fails
void finch_report_error(const FStr *msg) {
    fflush(stdout);
    fprintf(stderr, "%serror:%s %.*s\n", PAINT(2, "1;31"), PLAIN(2), (int)msg->len, msg->ptr ? msg->ptr : "");
    finch_str_drop((FStr *)msg);
}

void finch_str_sub(FStr *out, const FStr *s, int64_t start, int64_t end, const char *file, int64_t line) {
    if (start < 0 || end > s->len || start > end) {
        char msg[160];
        snprintf(msg, sizeof msg, "sub(%lld, %lld) is out of range (the length is %lld)", (long long)start,
                 (long long)end, (long long)s->len);
        finch_panic(file, line, msg);
    }
    str_heap(out, s->ptr + start, end - start);
}

int64_t finch_str_find(const FStr *s, const FStr *t) {
    if (t->len == 0) return 0;
    for (int64_t i = 0; i + t->len <= s->len; i++)
        if (s->ptr[i] == t->ptr[0] && memcmp(s->ptr + i, t->ptr, (size_t)t->len) == 0) return i;
    return -1;
}

int finch_str_starts(const FStr *s, const FStr *t) {
    return t->len <= s->len && memcmp(s->ptr, t->ptr, (size_t)t->len) == 0;
}

int finch_str_ends(const FStr *s, const FStr *t) {
    return t->len <= s->len && memcmp(s->ptr + s->len - t->len, t->ptr, (size_t)t->len) == 0;
}

void finch_str_trim(FStr *out, const FStr *s) {
    int64_t a = 0, b = s->len;
    while (a < b && isspace((unsigned char)s->ptr[a])) a++;
    while (b > a && isspace((unsigned char)s->ptr[b - 1])) b--;
    str_heap(out, s->ptr + a, b - a);
}

void finch_str_upper(FStr *out, const FStr *s) {
    str_heap(out, s->ptr, s->len);
    for (int64_t i = 0; i < out->len; i++) out->ptr[i] = (char)toupper((unsigned char)out->ptr[i]);
}

void finch_str_lower(FStr *out, const FStr *s) {
    str_heap(out, s->ptr, s->len);
    for (int64_t i = 0; i < out->len; i++) out->ptr[i] = (char)tolower((unsigned char)out->ptr[i]);
}

void finch_str_replace(FStr *out, const FStr *s, const FStr *from, const FStr *to) {
    if (from->len == 0) {
        str_heap(out, s->ptr, s->len);
        return;
    }
    int64_t count = 0;
    for (int64_t i = 0; i + from->len <= s->len;) {
        if (memcmp(s->ptr + i, from->ptr, (size_t)from->len) == 0) {
            count++;
            i += from->len;
        } else {
            i++;
        }
    }
    int64_t len = s->len + count * (to->len - from->len);
    char *m = finch_alloc(len + 1), *w = m;
    for (int64_t i = 0; i < s->len;) {
        if (i + from->len <= s->len && memcmp(s->ptr + i, from->ptr, (size_t)from->len) == 0) {
            memcpy(w, to->ptr, (size_t)to->len);
            w += to->len;
            i += from->len;
        } else {
            *w++ = s->ptr[i++];
        }
    }
    *w = 0;
    out->ptr = m;
    out->len = len;
    out->cap = len + 1;
}

static void arr_push_str(FArr *a, const char *p, int64_t len);

// "a,b,,c".split(",") -> ["a", "b", "", "c"]; split("") gives single characters.
void finch_str_split(FArr *out, const FStr *s, const FStr *sep) {
    out->ptr = NULL;
    out->len = out->cap = 0;
    if (sep->len == 0) {
        for (int64_t i = 0; i < s->len; i++) arr_push_str(out, s->ptr + i, 1);
        return;
    }
    int64_t start = 0;
    for (int64_t i = 0; i + sep->len <= s->len;) {
        if (memcmp(s->ptr + i, sep->ptr, (size_t)sep->len) == 0) {
            arr_push_str(out, s->ptr + start, i - start);
            i += sep->len;
            start = i;
        } else {
            i++;
        }
    }
    arr_push_str(out, s->ptr + start, s->len - start);
}

void finch_str_join(FStr *out, const FArr *parts, const FStr *sep) {
    const FStr *p = parts->ptr;
    int64_t len = 0;
    for (int64_t i = 0; i < parts->len; i++) len += p[i].len + (i ? sep->len : 0);
    char *m = finch_alloc(len + 1), *w = m;
    for (int64_t i = 0; i < parts->len; i++) {
        if (i) {
            memcpy(w, sep->ptr, (size_t)sep->len);
            w += sep->len;
        }
        memcpy(w, p[i].ptr, (size_t)p[i].len);
        w += p[i].len;
    }
    *w = 0;
    out->ptr = m;
    out->len = len;
    out->cap = len + 1;
}

void finch_str_repeat(FStr *out, const FStr *s, int64_t n) {
    if (n < 0) n = 0;
    int64_t len = s->len * n;
    char *m = finch_alloc(len + 1);
    for (int64_t i = 0; i < n; i++) memcpy(m + i * s->len, s->ptr, (size_t)s->len);
    m[len] = 0;
    out->ptr = m;
    out->len = len;
    out->cap = len + 1;
}

void finch_str_from_bytes(FStr *out, const FArr *bytes) { str_heap(out, bytes->ptr, bytes->len); }

// ---------- building text: print(x) and str(x) of arrays, maps and structs ----------
// A buffer starts as an empty str and grows on the heap.

void finch_buf_add(FStr *b, const char *p, int64_t n) {
    if (b->len + n + 1 > b->cap) {
        int64_t cap = b->cap > 0 ? b->cap * 2 : 32;
        while (cap < b->len + n + 1) cap *= 2;
        b->ptr = must(realloc(b->cap > 0 ? b->ptr : NULL, (size_t)cap));
        b->cap = cap;
    }
    if (n) memcpy(b->ptr + b->len, p, (size_t)n);
    b->len += n;
    b->ptr[b->len] = 0;
}

static void buf_fmt(FStr *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void buf_fmt(FStr *b, const char *fmt, ...) {
    char t[64];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(t, sizeof t, fmt, ap);
    va_end(ap);
    finch_buf_add(b, t, n);
}

void finch_buf_int(FStr *b, int64_t v) { buf_fmt(b, "%lld", (long long)v); }
void finch_buf_uint(FStr *b, uint64_t v) { buf_fmt(b, "%llu", (unsigned long long)v); }
void finch_buf_float(FStr *b, double v) {
    char t[64];
    finch_buf_add(b, t, fmt_float(t, sizeof t, v, 0));
}
void finch_buf_f32(FStr *b, double v) {
    char t[64];
    finch_buf_add(b, t, fmt_float(t, sizeof t, v, 1));
}
void finch_buf_bool(FStr *b, int v) { finch_buf_add(b, v ? "true" : "false", v ? 4 : 5); }
void finch_buf_ptr(FStr *b, const void *p) {
    if (p) buf_fmt(b, "0x%llx", (unsigned long long)(uintptr_t)p);
    else finch_buf_add(b, "null", 4);
}

void finch_buf_char(FStr *b, char c, int quoted) {
    if (quoted) finch_buf_add(b, "'", 1);
    finch_buf_add(b, &c, 1);
    if (quoted) finch_buf_add(b, "'", 1);
}

void finch_buf_str(FStr *b, const FStr *s, int quoted) {
    if (quoted) finch_buf_add(b, "\"", 1);
    finch_buf_add(b, s->ptr, s->len);
    if (quoted) finch_buf_add(b, "\"", 1);
}

// print the buffer and free it
void finch_buf_print(FStr *b) {
    if (b->len) fwrite(b->ptr, 1, (size_t)b->len, stdout);
    if (b->cap > 0) free(b->ptr);
}

// ---------- arrays ----------

void finch_arr_reserve(FArr *a, int64_t need, int64_t es) {
    if (need <= a->cap) return;
    int64_t cap = a->cap * 2;
    if (cap < 4) cap = 4;
    if (cap < need) cap = need;
    a->ptr = must(realloc(a->cap > 0 ? a->ptr : NULL, (size_t)(cap * es)));
    a->cap = cap;
}

void finch_arr_make(FArr *out, int64_t len, int64_t es) {
    if (len < 0) len = 0;
    out->ptr = len ? finch_zalloc(len * es) : NULL;
    out->len = len;
    out->cap = len;
}

// grow or shrink to `len`; new elements are zero
void finch_arr_resize(FArr *a, int64_t len, int64_t es) {
    if (len < 0) len = 0;
    finch_arr_reserve(a, len, es);
    if (len > a->len) memset((char *)a->ptr + a->len * es, 0, (size_t)((len - a->len) * es));
    a->len = len;
}

// a byte-for-byte copy of the elements (the compiler deep-copies owning elements afterwards)
void finch_arr_clone_raw(FArr *out, const FArr *a, int64_t es) {
    out->len = a->len;
    out->cap = a->len;
    out->ptr = a->len ? finch_alloc(a->len * es) : NULL;
    if (a->len) memcpy(out->ptr, a->ptr, (size_t)(a->len * es));
}

void finch_arr_free(FArr *a) {
    if (a->cap > 0) free(a->ptr);
}

// open a hole at i (0 <= i <= len), len grows by one
void finch_arr_insert_gap(FArr *a, int64_t i, int64_t es, const char *file, int64_t line) {
    if (i < 0 || i > a->len) finch_panic_index(file, line, i, a->len);
    finch_arr_reserve(a, a->len + 1, es);
    char *p = a->ptr;
    memmove(p + (i + 1) * es, p + i * es, (size_t)((a->len - i) * es));
    a->len++;
}

// close the hole at i (the element was already moved out), len shrinks by one
void finch_arr_remove_gap(FArr *a, int64_t i, int64_t es) {
    char *p = a->ptr;
    memmove(p + i * es, p + (i + 1) * es, (size_t)((a->len - i - 1) * es));
    a->len--;
}

static void arr_push_str(FArr *a, const char *p, int64_t len) {
    finch_arr_reserve(a, a->len + 1, (int64_t)sizeof(FStr));
    str_heap((FStr *)a->ptr + a->len, p, len);
    a->len++;
}

void finch_bytes_from_str(FArr *out, const FStr *s) {
    out->len = out->cap = s->len;
    out->ptr = s->len ? finch_alloc(s->len) : NULL;
    if (s->len) memcpy(out->ptr, s->ptr, (size_t)s->len);
}

// ---------- program arguments, input and files ----------

void finch_args(FArr *out, int argc, char **argv) {
    out->len = out->cap = argc;
    out->ptr = finch_alloc((int64_t)sizeof(FStr) * (argc ? argc : 1));
    for (int i = 0; i < argc; i++) str_heap((FStr *)out->ptr + i, argv[i], (int64_t)strlen(argv[i]));
}

// after main: the arguments go
void finch_args_free(FArr *a) {
    for (int64_t i = 0; i < a->len; i++) finch_str_drop((FStr *)a->ptr + i);
    finch_arr_free(a);
}

// input("Name? ") prints the prompt and reads one line, without the newline.
void finch_input(FStr *out, const FStr *prompt) {
    if (prompt->len) fwrite(prompt->ptr, 1, (size_t)prompt->len, stdout);
    fflush(stdout);
    // read one line of any length (getline isn't available everywhere)
    size_t size = 128, n = 0;
    char *line = finch_alloc((int64_t)size);
    int c, got = 0;
    while ((c = fgetc(stdin)) != EOF) {
        got = 1;
        if (c == '\n') break;
        if (n + 1 >= size) line = must(realloc(line, size *= 2));
        line[n++] = (char)c;
    }
    if (!got) {
        free(line);
        str_static(out, empty);
        return;
    }
    while (n > 0 && line[n - 1] == '\r') n--;
    line[n] = 0;
    out->ptr = line;
    out->len = (int64_t)n;
    out->cap = (int64_t)size;
}

int finch_file_exists(const FStr *path) {
    FILE *f = fopen(path->ptr, "rb");
    if (f) fclose(f);
    return f != NULL;
}

int finch_try_read_file(FStr *out, const FStr *path, FStr *err) {
    FILE *f = fopen(path->ptr ? path->ptr : "", "rb");
    if (!f) {
        file_error(err, "read", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *m = finch_alloc(size + 1);
    size_t got = fread(m, 1, (size_t)size, f);
    fclose(f);
    m[got] = 0;
    out->ptr = m;
    out->len = (int64_t)got;
    out->cap = size + 1;
    return 1;
}

void finch_read_file(FStr *out, const FStr *path, const char *file, int64_t line) {
    FILE *f = fopen(path->ptr ? path->ptr : "", "rb");
    if (!f) {
        char msg[300];
        snprintf(msg, sizeof msg, "can't read the file \"%.*s\": %s", (int)(path->len > 200 ? 200 : path->len),
                 path->ptr, strerror(errno));
        finch_panic(file, line, msg);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *m = finch_alloc(size + 1);
    size_t got = fread(m, 1, (size_t)size, f);
    fclose(f);
    m[got] = 0;
    out->ptr = m;
    out->len = (int64_t)got;
    out->cap = size + 1;
}

// shell("ls -l"): run a command, give back its exit code
int64_t finch_shell(const FStr *cmd) {
    fflush(stdout);
    int status = system(cmd->ptr ? cmd->ptr : "");
#ifdef _WIN32
    return status;  // Windows gives the exit code directly
#else
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
#endif
}

int finch_delete_file(const FStr *path) { return remove(path->ptr ? path->ptr : "") == 0; }

int finch_try_delete_file(const FStr *path, FStr *err) {
    if (remove(path->ptr ? path->ptr : "") == 0) return 1;
    file_error(err, "delete", path);
    return 0;
}

// stdin/stdout/stderr are macros on some systems (Windows), so Finch asks for them here
void *finch_std_stream(int i) { return i == 0 ? (void *)stdin : i == 1 ? (void *)stdout : (void *)stderr; }

int finch_write_file(const FStr *path, const FStr *text) {
    FILE *f = fopen(path->ptr ? path->ptr : "", "wb");
    if (!f) return 0;
    size_t put = text->len ? fwrite(text->ptr, 1, (size_t)text->len, f) : 0;
    return fclose(f) == 0 && put == (size_t)text->len;
}

int finch_try_write_file(const FStr *path, const FStr *text, FStr *err) {
    errno = 0;
    if (finch_write_file(path, text)) return 1;
    if (!errno) errno = EIO;
    file_error(err, "write", path);
    return 0;
}

// ---------- folders (std/fs.fch, through std/finch_fs.h) ----------

int finch_fs_is_dir(const char *path) {
#ifdef _WIN32
    struct _finddata_t d;
    intptr_t h = _findfirst(path, &d);
    if (h == -1) return 0;
    _findclose(h);
    return (d.attrib & _A_SUBDIR) != 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

int finch_fs_exists(const char *path) {
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

static int make_one_dir(const char *p) {
#ifdef _WIN32
    return _mkdir(p) == 0 || errno == EEXIST;
#else
    return mkdir(p, 0777) == 0 || errno == EEXIST;
#endif
}

// mkdir -p: the parents too; 1 if the folder is there afterwards
int finch_fs_mkdir(const char *path) {
    size_t n = strlen(path);
    char *p = finch_alloc((int64_t)n + 1);
    memcpy(p, path, n + 1);
    for (size_t i = 1; i < n; i++) {
        if (p[i] == '/' || p[i] == '\\') {
            if (p[i - 1] == ':') continue;  /* C: is a drive */
            char c = p[i];
            p[i] = 0;
            make_one_dir(p);
            p[i] = c;
        }
    }
    int ok = make_one_dir(p) && finch_fs_is_dir(p);
    free(p);
    return ok;
}

int finch_fs_remove_dir(const char *path) {
#ifdef _WIN32
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

// the names in a folder, one per call ("" at the end), without . and ..
typedef struct {
#ifdef _WIN32
    intptr_t h;
    struct _finddata_t d;
    int first;
#else
    DIR *d;
#endif
} FsList;

void *finch_fs_list_open(const char *path) {
    FsList *l = finch_zalloc((int64_t)sizeof(FsList));
#ifdef _WIN32
    size_t n = strlen(path);
    char *pat = finch_alloc((int64_t)n + 3);
    memcpy(pat, path, n);
    memcpy(pat + n, "\\*", 3);
    l->h = _findfirst(pat, &l->d);
    free(pat);
    if (l->h == -1) {
        free(l);
        return NULL;
    }
    l->first = 1;
#else
    l->d = opendir(path);
    if (!l->d) {
        free(l);
        return NULL;
    }
#endif
    return l;
}

const char *finch_fs_list_next(void *handle) {
    FsList *l = handle;
    for (;;) {
#ifdef _WIN32
        if (!l->first && _findnext(l->h, &l->d) != 0) return "";
        l->first = 0;
        const char *name = l->d.name;
#else
        struct dirent *e = readdir(l->d);
        if (!e) return "";
        const char *name = e->d_name;
#endif
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) return name;
    }
}

void finch_fs_list_close(void *handle) {
    FsList *l = handle;
#ifdef _WIN32
    _findclose(l->h);
#else
    closedir(l->d);
#endif
    free(l);
}

// where a program keeps its own files for this user (saves, settings); "" if unknown
//   Linux: $XDG_DATA_HOME or ~/.local/share   macOS: ~/Library/Application Support   Windows: %APPDATA%
const char *finch_fs_data_home(void) {
    static char buf[4096];
    const char *v;
#if defined(_WIN32)
    v = getenv("APPDATA");
    snprintf(buf, sizeof buf, "%s", v ? v : "");
#elif defined(__APPLE__)
    v = getenv("HOME");
    if (v) snprintf(buf, sizeof buf, "%s/Library/Application Support", v);
    else buf[0] = 0;
#else
    v = getenv("XDG_DATA_HOME");
    if (v && *v) snprintf(buf, sizeof buf, "%s", v);
    else if ((v = getenv("HOME"))) snprintf(buf, sizeof buf, "%s/.local/share", v);
    else buf[0] = 0;
#endif
    return buf;
}

// ---------- threads (std/thread.fch, through std/finch_thread.h) ----------

typedef struct {
    void (*fn)(void *);
    void *arg;
} ThreadStart;

#ifdef _WIN32
static DWORD WINAPI thread_main(LPVOID p) {
    ThreadStart st = *(ThreadStart *)p;
    free(p);
    st.fn(st.arg);
    return 0;
}
#else
static void *thread_main(void *p) {
    ThreadStart st = *(ThreadStart *)p;
    free(p);
    st.fn(st.arg);
    return NULL;
}
#endif

// run fn(arg) on a new thread; NULL if it couldn't start
void *finch_thread_spawn(void (*fn)(void *), void *arg) {
    ThreadStart *st = finch_alloc((int64_t)sizeof(ThreadStart));
    st->fn = fn;
    st->arg = arg;
#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, thread_main, st, 0, NULL);
    if (!h) {
        free(st);
        return NULL;
    }
    return h;
#else
    pthread_t *t = finch_alloc((int64_t)sizeof(pthread_t));
    if (pthread_create(t, NULL, thread_main, st) != 0) {
        free(st);
        free(t);
        return NULL;
    }
    return t;
#endif
}

// wait for the thread to end; 1 if that worked
int finch_thread_join(void *t) {
    if (!t) return 0;
#ifdef _WIN32
    int ok = WaitForSingleObject((HANDLE)t, INFINITE) == WAIT_OBJECT_0;
    CloseHandle((HANDLE)t);
    return ok;
#else
    int ok = pthread_join(*(pthread_t *)t, NULL) == 0;
    free(t);
    return ok;
#endif
}

void *finch_mutex_new(void) {
#ifdef _WIN32
    CRITICAL_SECTION *m = finch_alloc((int64_t)sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(m);
#else
    pthread_mutex_t *m = finch_alloc((int64_t)sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
#endif
    return m;
}

void finch_mutex_lock(void *m) {
#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION *)m);
#else
    pthread_mutex_lock((pthread_mutex_t *)m);
#endif
}

void finch_mutex_unlock(void *m) {
#ifdef _WIN32
    LeaveCriticalSection((CRITICAL_SECTION *)m);
#else
    pthread_mutex_unlock((pthread_mutex_t *)m);
#endif
}

void finch_mutex_free(void *m) {
    if (!m) return;
#ifdef _WIN32
    DeleteCriticalSection((CRITICAL_SECTION *)m);
#else
    pthread_mutex_destroy((pthread_mutex_t *)m);
#endif
    free(m);
}

int finch_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

// ---------- maps ----------
// map[K]V = { entries; len; used; cap; index; icap; adds }
//   entries: `used` slots of es bytes, each { uint64 hash; K key; V value }, in insertion order.
//            A removed entry keeps its slot (hash 0) until the next resize packs the slots.
//   index:   icap slots (a power of two, at least 2 * cap): -1 empty, -2 removed, else an entry number.
//   adds:    how many keys were ever added; a `for` over the map stops with an error if it changes.
// The compiler stores, copies and drops keys and values; the runtime only finds and places entries.
// Keys are compared byte for byte (numbers, chars, bools) or as text (kstr).

typedef struct {
    char *entries;
    int64_t len, used, cap;
    int64_t *index;
    int64_t icap;
    int64_t adds;
} FMap;

static uint64_t map_hash(const void *key, int64_t ks, int kstr) {
    const unsigned char *p;
    size_t n;
    if (kstr) {
        const FStr *s = key;
        p = (const unsigned char *)s->ptr;
        n = (size_t)s->len;
    } else {
        p = key;
        n = (size_t)ks;
    }
    uint64_t h = 1469598103934665603ULL;  // FNV-1a, then mixed so the low bits spread well
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    h ^= h >> 31;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 29;
    return h | 1;  // 0 marks a removed entry
}

static int map_key_eq(const char *a, const void *b, int64_t ks, int kstr) {
    if (!kstr) return memcmp(a, b, (size_t)ks) == 0;
    const FStr *x = (const FStr *)a, *y = b;
    return x->len == y->len && (x->len == 0 || memcmp(x->ptr, y->ptr, (size_t)x->len) == 0);
}

static void map_reindex(FMap *m, int64_t es) {
    if (m->len != m->used) {  // pack the slots of removed entries away
        int64_t j = 0;
        for (int64_t i = 0; i < m->used; i++) {
            char *e = m->entries + i * es;
            if (*(uint64_t *)e == 0) continue;
            if (i != j) memmove(m->entries + j * es, e, (size_t)es);
            j++;
        }
        m->used = j;
    }
    int64_t icap = 16;
    while (icap < m->cap * 2) icap *= 2;
    free(m->index);
    m->index = finch_alloc(icap * (int64_t)sizeof(int64_t));
    memset(m->index, 0xff, (size_t)icap * sizeof(int64_t));
    m->icap = icap;
    for (int64_t i = 0; i < m->used; i++) {
        uint64_t h = *(uint64_t *)(m->entries + i * es);
        int64_t s = (int64_t)(h & (uint64_t)(icap - 1));
        while (m->index[s] != -1) s = (s + 1) & (icap - 1);
        m->index[s] = i;
    }
}

// the entry number of `key`, or -1
int64_t finch_map_find(const FMap *m, const void *key, int64_t es, int64_t ks, int32_t kstr) {
    if (m->len == 0) return -1;
    uint64_t h = map_hash(key, ks, kstr);
    for (int64_t s = (int64_t)(h & (uint64_t)(m->icap - 1));; s = (s + 1) & (m->icap - 1)) {
        int64_t i = m->index[s];
        if (i == -1) return -1;
        if (i < 0) continue;
        char *e = m->entries + i * es;
        if (*(uint64_t *)e == h && map_key_eq(e + 8, key, ks, kstr)) return i;
    }
}

// The entry for `key`. A new one is zeroed except its hash (*added = 1); the compiler then stores the key.
int64_t finch_map_slot(FMap *m, const void *key, int64_t es, int64_t ks, int32_t kstr, int32_t *added) {
    int64_t i = finch_map_find(m, key, es, ks, kstr);
    *added = i < 0;
    if (i >= 0) return i;
    if (m->used == m->cap) {
        if (m->len * 4 < m->cap * 3) {  // many removed slots: pack them instead of growing
            map_reindex(m, es);
        } else {
            m->cap = m->cap ? m->cap * 2 : 8;
            m->entries = must(realloc(m->entries, (size_t)(m->cap * es)));
            map_reindex(m, es);
        }
    }
    uint64_t h = map_hash(key, ks, kstr);
    i = m->used++;
    char *e = m->entries + i * es;
    memset(e, 0, (size_t)es);
    *(uint64_t *)e = h;
    int64_t s = (int64_t)(h & (uint64_t)(m->icap - 1));
    while (m->index[s] >= 0) s = (s + 1) & (m->icap - 1);
    m->index[s] = i;
    m->len++;
    m->adds++;
    return i;
}

// Forget entry i (its key and value were already dropped by the compiler).
void finch_map_remove_at(FMap *m, int64_t i, int64_t es) {
    char *e = m->entries + i * es;
    uint64_t h = *(uint64_t *)e;
    int64_t s = (int64_t)(h & (uint64_t)(m->icap - 1));
    while (m->index[s] != i) s = (s + 1) & (m->icap - 1);
    m->index[s] = -2;
    *(uint64_t *)e = 0;
    if (--m->len == 0) {
        m->used = 0;
        memset(m->index, 0xff, (size_t)m->icap * sizeof(int64_t));
    }
}

void finch_map_clear(FMap *m) {
    m->len = m->used = 0;
    if (m->index) memset(m->index, 0xff, (size_t)m->icap * sizeof(int64_t));
}

void finch_map_free(FMap *m) {
    free(m->entries);
    free(m->index);
}

// A copy of the slots and the index; the compiler then copies the keys and values that own memory.
void finch_map_clone_raw(FMap *out, const FMap *m, int64_t es) {
    *out = *m;
    if (!m->cap) return;
    out->entries = finch_alloc(m->cap * es);
    memcpy(out->entries, m->entries, (size_t)(m->used * es));
    out->index = finch_alloc(m->icap * (int64_t)sizeof(int64_t));
    memcpy(out->index, m->index, (size_t)m->icap * sizeof(int64_t));
}

__attribute__((noreturn, cold)) void finch_map_changed(const char *file, int64_t line) {
    finch_panic(file, line, "a key was added to the map while looping over it (collect the new keys, and add them after the loop)");
}

__attribute__((noreturn, cold)) void finch_map_missing(const char *file, int64_t line, const FStr *key) {
    char msg[200];
    snprintf(msg, sizeof msg, "the key %.*s is not in the map (check with .has(key), or use .get(key, default))",
             (int)(key->len > 100 ? 100 : key->len), key->ptr ? key->ptr : "");
    finch_panic(file, line, msg);
}

// ---------- cpu_has("avx2"): what the processor running the program can do ----------
//
// x86: CPUID, and for the AVX and AVX-512 families also XGETBV (the system must save those registers, or the
// instructions fault). ARM64: the kernel's hwcaps on Linux and Android, sysctl on macOS. A name of the other
// architecture is false.

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
static int x86_leaf(unsigned leaf, unsigned sub, unsigned r[4]) {
    unsigned max = __get_cpuid_max(0, 0);
    if (leaf > max) return 0;
    __cpuid_count(leaf, sub, r[0], r[1], r[2], r[3]);
    return 1;
}
static unsigned long long x86_xcr0(void) {
    unsigned lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((unsigned long long)hi << 32) | lo;
}
static int finch_cpu_has_c(const char *n) {
    unsigned l1[4] = {0}, l7[4] = {0}, l71[4] = {0};
    x86_leaf(1, 0, l1);
    x86_leaf(7, 0, l7);
    x86_leaf(7, 1, l71);
    int osx = (l1[2] >> 27) & 1;                                    // OSXSAVE
    unsigned long long xcr0 = osx ? x86_xcr0() : 0;
    int ymm = osx && (xcr0 & 6) == 6;                               // the system saves xmm and ymm
    int zmm = ymm && (xcr0 & 0xe0) == 0xe0;                         // and the AVX-512 registers
    if (!strcmp(n, "sse3")) return l1[2] & 1;
    if (!strcmp(n, "ssse3")) return (l1[2] >> 9) & 1;
    if (!strcmp(n, "sse4.1")) return (l1[2] >> 19) & 1;
    if (!strcmp(n, "sse4.2")) return (l1[2] >> 20) & 1;
    if (!strcmp(n, "popcnt")) return (l1[2] >> 23) & 1;
    if (!strcmp(n, "avx")) return ymm && ((l1[2] >> 28) & 1);
    if (!strcmp(n, "fma")) return ymm && ((l1[2] >> 12) & 1);
    if (!strcmp(n, "f16c")) return ymm && ((l1[2] >> 29) & 1);
    if (!strcmp(n, "avx2")) return ymm && ((l7[1] >> 5) & 1);
    if (!strcmp(n, "bmi1")) return (l7[1] >> 3) & 1;
    if (!strcmp(n, "bmi2")) return (l7[1] >> 8) & 1;
    if (!strcmp(n, "avxvnni")) return ymm && ((l71[0] >> 4) & 1);
    if (!strcmp(n, "avx512f")) return zmm && ((l7[1] >> 16) & 1);
    if (!strcmp(n, "avx512dq")) return zmm && ((l7[1] >> 17) & 1);
    if (!strcmp(n, "avx512bw")) return zmm && ((l7[1] >> 30) & 1);
    if (!strcmp(n, "avx512vl")) return zmm && ((l7[1] >> 31) & 1);
    if (!strcmp(n, "avx512vnni")) return zmm && ((l7[2] >> 11) & 1);
    if (!strcmp(n, "avx512fp16")) return zmm && ((l7[3] >> 23) & 1);
    if (!strcmp(n, "avx512bf16")) return zmm && ((l71[0] >> 5) & 1);
    return 0;
}
#elif defined(__aarch64__) && defined(__APPLE__)
#include <sys/sysctl.h>
static int sysctl_flag(const char *name) {
    int v = 0;
    size_t len = sizeof v;
    if (sysctlbyname(name, &v, &len, NULL, 0) != 0) return 0;
    return v != 0;
}
static int finch_cpu_has_c(const char *n) {
    if (!strcmp(n, "neon")) return 1;
    if (!strcmp(n, "fp16")) return sysctl_flag("hw.optional.arm.FEAT_FP16");
    if (!strcmp(n, "dotprod")) return sysctl_flag("hw.optional.arm.FEAT_DotProd");
    if (!strcmp(n, "i8mm")) return sysctl_flag("hw.optional.arm.FEAT_I8MM");
    if (!strcmp(n, "bf16")) return sysctl_flag("hw.optional.arm.FEAT_BF16");
    return 0;                                                       // no SVE on Apple chips
}
#elif defined(__aarch64__) && defined(__linux__)
#include <sys/auxv.h>
static int finch_cpu_has_c(const char *n) {
    unsigned long h = getauxval(AT_HWCAP), h2 = getauxval(AT_HWCAP2);
    if (!strcmp(n, "neon")) return 1;
    if (!strcmp(n, "fp16")) return (h >> 10) & 1;                    // HWCAP_ASIMDHP
    if (!strcmp(n, "dotprod")) return (h >> 20) & 1;                 // HWCAP_ASIMDDP
    if (!strcmp(n, "sve")) return (h >> 22) & 1;                     // HWCAP_SVE
    if (!strcmp(n, "sve2")) return (h2 >> 1) & 1;                    // HWCAP2_SVE2
    if (!strcmp(n, "i8mm")) return (h2 >> 13) & 1;                   // HWCAP2_I8MM
    if (!strcmp(n, "bf16")) return (h2 >> 14) & 1;                   // HWCAP2_BF16
    return 0;
}
#else
static int finch_cpu_has_c(const char *n) {
    (void)n;
    return 0;
}
#endif

int finch_cpu_has(const FStr *name) {
    char n[32];
    int64_t len = name->len < 31 ? name->len : 31;
    memcpy(n, name->ptr ? name->ptr : "", (size_t)len);
    n[len] = 0;
    return finch_cpu_has_c(n) ? 1 : 0;
}
