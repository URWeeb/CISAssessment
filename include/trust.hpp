#pragma once

#include <chrono>

namespace publisher::trust {
    using Duration = std::chrono::duration<double>;

    double Decayed(double weight, Duration elapsed, Duration half_life);
}  // namespace publisher::trust