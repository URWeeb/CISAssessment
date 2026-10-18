#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace publisher {

    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    enum class ClientLevel {
        STAFF, FREELANCER
    };

    enum class ReceiptStatus {
        ACCEPTED, DUPLICATE, REJECTED
    };

    enum class ServerEventType {
        CONGRATULATION;
    };

    struct Client {
        std::string id;
        std::string name;
        ClientLevel lvl = ClientLevel::FREELANCER;
        double trust = 0.0;
        bool is_editor = false;
    };

    struct StoryId {
        std::string category;
        std::string topic;

        std::string Composite() const {
            return category + "\x1f" + topic;
        }

        bool operator==(const StoryId& other) const {
            return category == other.category && topic == other.topic;
        }
    };

    struct Contribution {
        std::string client_id;
        TimePoint receiving_time;
        int rank = 0;
    };

    struct Story {
        StoryId id;
        std::vector<Contribution> contributions;
        std::vector<Contribution> publish_order;
        bool published = false;
        TimePoint published_at{};

        bool HasContribution(const std::string& client_id) const {
            for (const auto& contribution : contributions) {
                if (contribution.client_id == client_id) {
                    return true;
                }
            }

            return false;
        }  
    };

    struct Receipt {
        ReceiptStatus status = ReceiptStatus::REJECTED; 
        std::string reason;
        bool credited = false;
        bool story_published_initiator = false;
        int rank = 0;
    };

    struct Note {
        std::string client_id;
        StoryId id;
        std::string idempotency_key;
    };

    struct ServerEvent {
        std::uint64_t seq = 0;
        ServerEventType type = ServerEventType::Congratulation;
        TimePoint created_at{};
        std::string payload;
    };

    struct ClientStats {
        std::string client_id;
        int accepted_counter = 0;
        int stories_touched = 0;
        int published_counter = 0;
        double share = 0.0;
        int sensations = 0;
        double average_rank = 0.0;
    };
}