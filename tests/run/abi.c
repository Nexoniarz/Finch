#include "abi.h"
#include <string.h>
Vec2 vec2_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
Vec3 vec3_scale(Vec3 v, float k) { return (Vec3){v.x * k, v.y * k, v.z * k}; }
Color color_invert(Color c) { return (Color){255 - c.r, 255 - c.g, 255 - c.b, c.a}; }
Mixed mixed_bump(Mixed m) { return (Mixed){m.d + 0.5, m.i + 1}; }
Pair pair_swap(Pair p) { return (Pair){p.b, p.a}; }
Big big_sum(Big a, Big b) { return (Big){a.a + b.a, a.b + b.b, a.c + b.c}; }
int64_t big_total(Big b) { return b.a + b.b + b.c; }
int named_len(Named n) { return (int)strlen(n.name) * 100 + n.count; }
Sprite sprite_move(Sprite s, float dx) { s.pos.x += dx; s.color.a = 7; return s; }
int item_sum(Item *it) { int s = 0; for (; it; it = it->next) s += it->value; return s; }
double many(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Vec2 e, Vec2 f, Vec2 g, Vec2 h, Vec2 i, Pair p1, Pair p2, Pair p3, Pair p4) {
    return a.x + b.x + c.x + d.x + e.x + f.x + g.x + h.x + i.x + i.y + p1.a + p2.b + p3.a + p4.b;
}
int64_t apply_pair(Pair (*fn)(Pair, int64_t), Pair p, int64_t k) { Pair r = fn(p, k); return r.a * 1000 + r.b; }
float apply_vec(Vec2 (*fn)(Vec2), Vec2 v) { Vec2 r = fn(v); return r.x + r.y; }
