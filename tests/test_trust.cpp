#include <gtest/gtest.h>

#include <chrono>

#include "../include/trust.hpp"

using namespace std::chrono_literals;
using publisher::trust::Decayed;

TEST(Trust, AtHalfLifeReturnsHalfWeight) {
    EXPECT_NEAR(Decayed(1.0, 6h, 6h), 0.5, 1e-9);
}

TEST(Trust, NonPositiveElapsedOrHalfLifeReturnsWeightUnchanged) {
    EXPECT_NEAR(Decayed(1.0, 0h, 6h), 1.0, 1e-9);
    EXPECT_NEAR(Decayed(1.0, -1h, 6h), 1.0, 1e-9);
    EXPECT_NEAR(Decayed(1.0, 1h, 0h), 1.0, 1e-9);
}
