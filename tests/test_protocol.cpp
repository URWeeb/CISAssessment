#include <gtest/gtest.h>

#include "../include/protocol.hpp"

using publisher::protocol::JoinTab;
using publisher::protocol::SplitTab;

TEST(Protocol, SplitTabJoinTabRoundTripWithSpacesInFields) {
    std::vector<std::string> fields = {"NOTE", "alice", "k1", "двор", "тема с пробелами"};
    auto joined = JoinTab(fields);
    auto back = SplitTab(joined);
    ASSERT_EQ(back.size(), fields.size());
    
    for (size_t i = 0; i < fields.size(); ++i){
        EXPECT_EQ(back[i], fields[i]);
    }
}
