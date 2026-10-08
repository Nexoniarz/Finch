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

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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

static void bad_number(const FStr *s, const char *kind, const char *file, int64_t line) {
    char msg[200];
    snprintf(msg, sizeof msg, "can't turn \"%.*s\" into %s", (int)(s->len > 100 ? 100 : s->len), s->ptr, kind);
    finch_panic(file, line, msg);
}

int64_t finch_str_to_int(const FStr *s, const char *file, int64_t line) {
    const char *p = s->ptr;
    while (isspace((unsigned char)*p)) p++;
    char *end;
    errno = 0;
    long long v = strtoll(p, &end, 10);
    while (isspace((unsigned char)*end)) end++;
    if (end == p || *end || errno == ERANGE) bad_number(s, "int", file, line);
    return v;
}

double finch_str_to_float(const FStr *s, const char *file, int64_t line) {
    const char *p = s->ptr;
    while (isspace((unsigned char)*p)) p++;
    char *end;
    double v = strtod(p, &end);
    while (isspace((unsigned char)*end)) end++;
    if (end == p || *end) bad_number(s, "float", file, line);
    return v;
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
    char *line = NULL;
    size_t size = 0;
    ssize_t n = getline(&line, &size, stdin);
    if (n < 0) {
        free(line);
        str_static(out, empty);
        return;
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
    out->ptr = line;
    out->len = n;
    out->cap = (int64_t)size;
}

int finch_file_exists(const FStr *path) {
    FILE *f = fopen(path->ptr, "rb");
    if (f) fclose(f);
    return f != NULL;
}

void finch_read_file(FStr *out, const FStr *path, const char *file, int64_t line) {
    FILE *f = fopen(path->ptr, "rb");
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
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

int finch_write_file(const FStr *path, const FStr *text) {
    FILE *f = fopen(path->ptr, "wb");
    if (!f) return 0;
    size_t put = fwrite(text->ptr, 1, (size_t)text->len, f);
    return fclose(f) == 0 && put == (size_t)text->len;
}
