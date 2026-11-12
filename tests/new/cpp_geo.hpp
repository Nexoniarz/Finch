// A small C++ library for tests/new/cpp_import.fch: namespaces, a class, overloads, defaults, references, exceptions.
#pragma once
#include <stdexcept>
#include <string>

struct Vec2 {
    float x;
    float y;
};

enum class Mode { Fast = 1, Careful = 2 };

namespace geo {
    inline float length2(Vec2 v) { return v.x * v.x + v.y * v.y; }
    inline Vec2 scaled(const Vec2 &v, float by = 2.0f) { return Vec2{v.x * by, v.y * by}; }
    inline int add(int a, int b) { return a + b; }
    inline double add(double a, double b) { return a + b + 0.5; }
    inline void grow(float &x) { x *= 10.0f; }
    inline int checked(int x) {
        if (x < 0) throw std::invalid_argument("negative number");
        return x;
    }
    inline int mode_value(Mode m) { return (int)m; }
    inline std::string greeting() { return "hi"; }
}

class Counter {
public:
    Counter(int start) : n(start) {}
    ~Counter() {}
    void add(int by = 1) { n += by; }
    int value() const { return n; }
    static int twice(int x) { return 2 * x; }
private:
    int n;
};

inline int counter_total(const Counter &c) { return c.value(); }
