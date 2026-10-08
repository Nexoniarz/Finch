#include <stdint.h>
typedef struct { float x, y; } Vec2;                      // SSE: <2 x float>
typedef struct { float x, y, z; } Vec3;                   // SSE, SSE (float tail)
typedef struct { uint8_t r, g, b, a; } Color;             // INTEGER: i32
typedef struct { double d; int i; } Mixed;                // SSE + INTEGER
typedef struct { int64_t a, b; } Pair;                    // INTEGER, INTEGER
typedef struct { int64_t a, b, c; } Big;                  // MEMORY (24 bytes)
typedef struct { char name[8]; int count; } Named;        // fixed array inside
typedef struct { Vec2 pos; Color color; } Sprite;         // nested
typedef struct Item { int value; struct Item *next; } Item;

Vec2 vec2_add(Vec2 a, Vec2 b);
Vec3 vec3_scale(Vec3 v, float k);
Color color_invert(Color c);
Mixed mixed_bump(Mixed m);
Pair pair_swap(Pair p);
Big big_sum(Big a, Big b);
int64_t big_total(Big b);
int named_len(Named n);
Sprite sprite_move(Sprite s, float dx);
int item_sum(Item *it);
// many structs: runs out of registers, later ones go on the stack
double many(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Vec2 e, Vec2 f, Vec2 g, Vec2 h, Vec2 i, Pair p1, Pair p2, Pair p3, Pair p4);
int64_t apply_pair(Pair (*fn)(Pair, int64_t), Pair p, int64_t k);
float apply_vec(Vec2 (*fn)(Vec2), Vec2 v);
