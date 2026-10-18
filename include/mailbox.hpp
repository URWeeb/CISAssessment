#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain.hpp"

namespace publisher::mailbox {

    class Box {
    public:
        std::uint64_t Append(publisher::ServerEvent event);
        std::vector<publisher::ServerEvent> After(std::uint64_t after) const;

    private:
        mutable std::mutex mtex_;
        std::vector<publisher::ServerEvent> events_;
        std::uint64_t next_seq_ = 1;
    };

    class Registry {
    public:
        Box& BoxFor(const std::string& client_id);

    private:
        mutable std::mutex mtex_;
        std::unordered_map<std::string, std::unique_ptr<Box>> boxes_;
    };

}  // namespace publisher::mailbox