#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "../include/store.hpp"

using namespace std::chrono_literals;
using namespace publisher;
using publisher::store::Store;

namespace {
    constexpr auto kHalfLife = 6h;
}  // namespace

TEST(Store, SubmitContributionOneVotePerClient) {
    Store s;
    s.RegisterClient(Client{"alice", "", ClientLevel::FREELANCER, 0.1, false});
    StoryId id{"двор", "король уехал"};
    auto now = Clock::now();

    auto r1 = s.SubmitContribution(id, "alice", now, kHalfLife, 10.0);
    EXPECT_EQ(r1.rank, 1);
    EXPECT_TRUE(r1.credited);

    auto r2 = s.SubmitContribution(id, "alice", now + 1min, kHalfLife, 10.0);
    EXPECT_FALSE(r2.credited);
    EXPECT_EQ(r2.rank, r1.rank);
    EXPECT_EQ(r2.story.contributions.size(), 1u);
}

TEST(Store, SubmitContributionConcurrentSubmissionsPublishExactlyOnce) {
    Store s;
    StoryId id{"двор", "много источников"};
    constexpr int n = 50;
    
    for (int i = 0; i < n; ++i) {
        s.RegisterClient(Client{"c" + std::to_string(i), "", ClientLevel::FREELANCER, 1.0, false});
    }

    auto now = Clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> published_count{0};
    threads.reserve(n);

    for (int i = 0; i < n; ++i) {
        threads.emplace_back([&, i]() {
            auto r = s.SubmitContribution(id, "c" + std::to_string(i), now, kHalfLife, 0.5);
            
            if (r.published_now) {
                published_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(published_count.load(), 1);
}
