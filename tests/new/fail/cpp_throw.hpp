// for cpp_exception.fch: a C++ function that throws
#pragma once
#include <stdexcept>

namespace checks {
    inline int positive(int x) {
        if (x < 0) throw std::invalid_argument("negative number");
        return x;
    }
}
