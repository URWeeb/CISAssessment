#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain.hpp"

namespace publisher::store {

    struct SubmitResult {
        Story story;
        int rank = 0;
        bool credited = false;
        bool published_now = false;
        double trust_sum = 0.0;
    };

    class Store {
    public:
        void RegisterClient(const Client& client);

        std::optional<Client> GetClient(const std::string& client_id) const;

        std::vector<Client> AllClients() const;

        void MarkHandshake(const std::string& id);

        bool IsHandshaken(const std::string& id) const;

        std::optional<Receipt> FindIdempotent(const std::string& client_id,
                                              const std::string& idempotency_key) const;

        void SaveIdempotent(const std::string& client_id, const std::string& idempotency_key,
                            const Receipt& receipt);

        void IncAccepted(const std::string& client_id);

        int AcceptedCount(const std::string& client_id) const;

        Story StorySnapshot(const StoryId& id) const;

        SubmitResult SubmitContribution(const StoryId& id, const std::string& client_id,
                                        TimePoint now, std::chrono::duration<double> half_life,
                                        double threshold);

        std::vector<Story> AllStories() const;

        std::vector<Story> AllPublishedStories() const;

    private:
        mutable std::mutex mu_;
        std::unordered_map<std::string, Client> clients_;
        std::unordered_map<std::string, bool> handshaken_;
        std::unordered_map<std::string, Story> stories_;
        std::unordered_map<std::string, Receipt> idempotency_;
        std::unordered_map<std::string, int> accepted_count_;

    private:
        Story& StoryRefLocked(const StoryId& id);
    };

}  // namespace publisher::store