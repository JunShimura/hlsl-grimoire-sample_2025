#include "stdafx.h"
#include <limits>
#include <cstdint>
class tesutSource {
    int TestFunc() {
        bool test = (100 <= std::numeric_limits<uint16_t>::max());
    }
}