#include "../include/service.hpp"

#include <algorithm>
#include <sstream>

namespace publisher::service {

    Service::Service(store::Store& st, mailbox::Registry& mb, Config cfg,
                     std::function<TimePoint()> clock)
        : store_(st), mailbox_(mb), cfg_(cfg), clock_(std::move(clock)) {}

    void Service::RegisterClient(const Client& c) { store_.RegisterClient(c); }

    void Service::Handshake(const std::string& client_id) {
        if (!store_.GetClient(client_id)) {
            throw ServiceError(ErrorCode::UNKNOWN_CLIENT,
                               "неизвестный клиент: не заведён редакцией");
        }

        store_.MarkHandshake(client_id);
    }

    Receipt Service::SubmitNote(const Note& note) {
        if (!store_.GetClient(note.client_id)) {
            throw ServiceError(ErrorCode::UNKNOWN_CLIENT,
                               "неизвестный клиент: не заведён редакцией");
        }
        if (!store_.IsHandshaken(note.client_id)) {
            throw ServiceError(ErrorCode::HANDSHAKE_REQUIRED,
                               "рукопожатие обязательно до рабочих сообщений");
        }

        if (auto cached = store_.FindIdempotent(note.client_id, note.idempotency_key)) {
            Receipt dup = *cached;
            dup.status = ReceiptStatus::DUPLICATE;
            return dup;
        }

        const TimePoint now = clock_();
        auto result =
            store_.SubmitContribution(note.id, note.client_id, now, cfg_.half_life, cfg_.threshold);

        Receipt receipt;
        receipt.status = ReceiptStatus::ACCEPTED;
        receipt.reason = "";
        receipt.credited = result.credited;
        receipt.story_published_initiator = result.published_now;
        receipt.rank = result.rank;

        store_.IncAccepted(note.client_id);
        store_.SaveIdempotent(note.client_id, note.idempotency_key, receipt);

        if (result.published_now) {
            NotifyPublication(result.story);
        }

        return receipt;
    }

    void Service::NotifyPublication(const Story& story) {
        int top = cfg_.congratulate_top;

        if (top > static_cast<int>(story.publish_order.size())) {
            top = static_cast<int>(story.publish_order.size());
        }

        for (int i = 0; i < top; ++i) {
            const auto& c = story.publish_order[static_cast<size_t>(i)];
            std::ostringstream msg;
            msg << "Поздравляем, ваша заметка попала в печать. Вы были " << c.rank
                << "-м, кто сказал нам об этой новости.";
            ServerEvent evt;
            evt.type = ServerEventType::CONGRATULATION;
            evt.created_at = clock_();
            evt.payload = msg.str();
            mailbox_.BoxFor(c.client_id).Append(std::move(evt));
        }
    }

    std::vector<ServerEvent> Service::Events(const std::string& client_id, std::uint64_t after) {
        return mailbox_.BoxFor(client_id).After(after);
    }

    ClientStats Service::StatsFor(const std::string& client_id) const {
        if (!store_.GetClient(client_id)) {
            throw ServiceError(ErrorCode::UNKNOWN_CLIENT,
                               "неизвестный клиент: не заведён редакцией");
        }

        return ComputeStats(client_id);
    }

    std::vector<ClientStats> Service::AllStats(const std::string& requester_id) const {
        auto requester = store_.GetClient(requester_id);

        if (!requester) {
            throw ServiceError(ErrorCode::UNKNOWN_CLIENT,
                               "неизвестный клиент: не заведён редакцией");
        }

        if (!requester->is_editor) {
            throw ServiceError(ErrorCode::FORBIDDEN,
                               "доступ запрещён: статистика по всем доступна только редактору");
        }

        std::vector<ClientStats> out;

        for (const auto& c : store_.AllClients()) {
            out.push_back(ComputeStats(c.id));
        }

        return out;
    }

    namespace {
        std::optional<int> CreditedRank(const Story& st, const std::string& client_id) {
            for (const auto& c : st.contributions) {
                if (c.client_id == client_id) {
                    return c.rank;
                }
            }

            return std::nullopt;
        }
    }  // namespace

    ClientStats Service::ComputeStats(const std::string& client_id) const {
        ClientStats stats;
        stats.client_id = client_id;
        stats.accepted_counter = store_.AcceptedCount(client_id);
        int touched = 0;
        int published = 0;
        int sensations = 0;
        int rank_sum = 0;
        int rank_count = 0;

        for (const auto& st : store_.AllStories()) {
            auto rank = CreditedRank(st, client_id);

            if (!rank) {
                continue;
            }

            touched++;

            if (st.published) {
                published++;
                rank_sum += *rank;
                rank_count++;

                if (*rank == 1) {
                    sensations++;
                }
            }
        }

        stats.stories_touched = touched;
        stats.published_counter = published;
        stats.sensations = sensations;

        if (touched > 0) {
            stats.share = static_cast<double>(published) / static_cast<double>(touched);
        }

        if (rank_count > 0) {
            stats.average_rank = static_cast<double>(rank_sum) / static_cast<double>(rank_count);
        }

        return stats;
    }

}  // namespace publisher::service