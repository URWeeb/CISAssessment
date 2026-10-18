#pragma once

#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "domain.hpp"
#include "mailbox.hpp"
#include "store.hpp"

namespace publisher::service {

    enum class ErrorCode { NONE, UNKNOWN_CLIENT, HANDSHAKE_REQUIRED, FORBIDDEN };

    struct Config {
        double threshold = 0.75;
        std::chrono::duration<double> half_life = std::chrono::hours(6);
        int congratulate_top = 3;
    };

    struct ServiceError : std::runtime_error {
    public:
        ErrorCode code;

    public:
        explicit ServiceError(ErrorCode c, const std::string& msg)
            : std::runtime_error(msg), code(c) {}
    };

    class Service {
    public:
        explicit Service(
            store::Store& st, mailbox::Registry& mb, Config cfg,
            std::function<TimePoint()> clock = []() { return Clock::now(); });

        void RegisterClient(const Client& c);

        void Handshake(const std::string& client_id);

        Receipt SubmitNote(const Note& note);

        std::vector<ServerEvent> Events(const std::string& client_id, std::uint64_t after);

        ClientStats StatsFor(const std::string& client_id) const;

        std::vector<ClientStats> AllStats(const std::string& requester_id) const;

    private:
        store::Store& store_;
        mailbox::Registry& mailbox_;
        Config cfg_;
        std::function<TimePoint()> clock_;

    private:
        void NotifyPublication(const Story& story);
        ClientStats ComputeStats(const std::string& client_id) const;
    };

}  // namespace publisher::service