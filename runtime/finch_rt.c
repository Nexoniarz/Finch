// The Finch runtime: string and array helpers, input, files and runtime errors.
// It is embedded in the compiler, compiled once into a cache folder, and linked
// statically into every Finch program. Everything here is plain C with no state.
//
// Layout shared with the compiler (src/codegen.cpp):
//   str  = { char *ptr; int64 len; int64 cap }   ptr is always NUL-terminated
//          cap > 0  : heap memory owned by the value (cap bytes allocated)
//          cap == 0 : a string literal, valid forever, never written to
//          cap == -1: borrowed from C, must be copied before it is kept
//   []T  = { T *ptr;   int64 len; int64 cap }    owns ptr when cap > 0
//   map[K]V: see "maps" below

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
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

__attribute__((noreturn, cold)) void finch_panic(const char *file, int64_t line, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "%s:%lld: runtime error: %s\n", file, (long long)line, msg);
    exit(1);
}

__attribute__((noreturn, cold)) void finch_panic_index(const char *file, int64_t line, int64_t i, int64_t len) {
    char msg[128];
    snprintf(msg, sizeof msg, "index %lld is out of range (the length is %lld)", (long long)i, (long long)len);
    finch_panic(file, line, msg);
}

static void *must(void *p) {
    if (!p) {
        fflush(stdout);
        fprintf(stderr, "runtime error: out of memory\n");
        exit(1);
    }
    return p;
}

void *finch_alloc(int64_t n) { return must(malloc(n ? (size_t)n : 1)); }
void *finch_zalloc(int64_t n) { return must(calloc(1, n ? (size_t)n : 1)); }

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

void finch_str_from_float(FStr *out, double v) {
    char buf[64];
    int n = snprintf(buf, sizeof buf, "%g", v);
    str_heap(out, buf, n);
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
    fprintf(stderr, "error: %.*s\n", (int)msg->len, msg->ptr ? msg->ptr : "");
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
void finch_buf_float(FStr *b, double v) { buf_fmt(b, "%g", v); }
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
