#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "../include/mailbox.hpp"

using namespace publisher;
using publisher::mailbox::Box;

TEST(Mailbox, BoxAppendIsConcurrencySafe) {
    Box box;
    constexpr int n = 100;
    std::vector<std::thread> threads;
    threads.reserve(n);

    for (int i = 0; i < n; ++i) {
        threads.emplace_back([&box]() {
            ServerEvent evt;
            evt.type = ServerEventType::CONGRATULATION;
            evt.payload = "x";
            box.Append(std::move(evt));
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto all = box.After(0);
    ASSERT_EQ(all.size(), static_cast<size_t>(n));

    std::vector<bool> seen(static_cast<size_t>(n) + 1, false);
    
    for (const auto& e : all) {
        ASSERT_GE(e.seq, 1u);
        ASSERT_LE(e.seq, static_cast<std::uint64_t>(n));
        EXPECT_FALSE(seen[e.seq]);
        seen[e.seq] = true;
    }
}
