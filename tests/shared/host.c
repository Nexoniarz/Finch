#include <dlfcn.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf jb;
void host_print(const char *s) { printf("host_print: %s\n", s); }
static void onFatal(const char *m) { printf("caught: %s\n", m); longjmp(jb, 1); }
int main(int argc, char **argv) {
    void *h = dlopen(argc > 1 ? argv[1] : "./libmod.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) { printf("%s\n", dlerror()); return 1; }
    ((void (*)(void (*)(const char *)))dlsym(h, "finch_module_on_fatal"))(onFatal);
    printf("add %ld\n", ((long (*)(long, long))dlsym(h, "add"))(2, 40));
    printf("even %d %d\n", ((_Bool (*)(long))dlsym(h, "is_even"))(4), ((_Bool (*)(long))dlsym(h, "is_even"))(5));
    ((void (*)(const char *))dlsym(h, "hello"))("Rakya");
    if (!setjmp(jb)) ((long (*)(long))dlsym(h, "crash"))(7);
    printf("still alive\n");
    return 0;
}
