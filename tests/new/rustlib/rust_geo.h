// The C side of tests/new/rustlib (what cbindgen would write)
#include <stdint.h>

typedef struct Point {
    int32_t x;
    int32_t y;
} Point;

float path_len(float x, float y);
int32_t manhattan(Point a, Point b);
uint32_t count_words(const char *text);
