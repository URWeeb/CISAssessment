#include "../include/trust.hpp"

#include <cmath>

namespace publisher::trust {

    double Decayed(double weight, Duration elapsed, Duration half_life) {
        const double elapsed_seconds = elapsed.count();
        const double half_life_seconds = half_life.count();

        if (elapsed_seconds <= 0.0 || half_life_seconds <= 0.0) {
            return weight;
        }

        return weight * std::exp2(-elapsed_seconds / half_life_seconds);
    }

}  // namespace publisher::trust