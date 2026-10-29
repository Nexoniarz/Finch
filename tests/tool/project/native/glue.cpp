// C++ behind a C header: extern "C" gives the function a name C (and Finch) can call
#include "glue.h"
#include <numeric>
#include <vector>

extern "C" int glue_answer(void) {
    std::vector<int> v = {1, 1};
    return GLUE_BASE + std::accumulate(v.begin(), v.end(), 0);
}
