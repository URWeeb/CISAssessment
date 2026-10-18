#include "../include/mailbox.hpp"

namespace publisher::mailbox {

    std::uint64_t Box::Append(publisher::ServerEvent evt) {
        std::lock_guard<std::mutex> lock(mtex_);

        evt.seq = next_seq_++;
        events_.push_back(std::move(evt));

        return events_.back().seq;
    }

    std::vector<publisher::ServerEvent> Box::After(std::uint64_t after) const {
        std::lock_guard<std::mutex> lock(mtex_);

        std::vector<publisher::ServerEvent> out;

        for (const auto& e : events_) {
            if (e.seq > after) out.push_back(e);
        }

        return out;
    }

    Box& Registry::BoxFor(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(mtex_);

        auto it = boxes_.find(client_id);

        if (it == boxes_.end()) {
            it = boxes_.emplace(client_id, std::make_unique<Box>()).first;
        }

        return *it->second;
    }

}  // namespace publisher::mailbox