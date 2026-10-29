// C unions and #define constants that are expressions (tests/new/c_unions_macros.fch)
#include <stdint.h>

typedef struct { uint32_t type; float x, y; } Motion;
typedef struct { uint32_t type; int32_t key; } Key;
typedef union {
    uint32_t type;
    Motion motion;
    Key key;
    uint8_t padding[64];
} Event;

typedef struct { int kind; union { int i; float f; } as; } Tagged;

#define EV_MOTION 0x400u
#define EV_KEY (EV_MOTION + 1)
#define FLAG_BIG ((uint64_t)1 << 40)
#define FLAG_GL 0x2ULL
#define FLAGS_BOTH (FLAG_BIG | FLAG_GL)
#define HALF (1.0 / 2)
#define GREETING "hi\tthere"
#define LONG_NAME "a" "b"
#define NOT_A_CONSTANT do { } while (0)

typedef struct { unsigned char r, g, b, a; } Color;
typedef struct { float x, y; } Vec2;
#define CLITERAL(type) (type)
#define RAYWHITE CLITERAL(Color){ 245, 245, 245, 255 }
#define DARK (Color){ 20, 20, 20 }
#define HALF_UP (Vec2){ 0.5f, -1 }
#define NOT_A_STRUCT_CONST (Color){ 1, 2, (unsigned char)(long)&poll_event, 4 }

static inline int fill_key(Event *e, int key) {
    e->key.type = EV_KEY;
    e->key.key = key;
    return 1;
}
int poll_event(Event *e);

static inline int twice(int x) { return 2 * x; }
static inline int sum_all(int n, ...) { return n; }
