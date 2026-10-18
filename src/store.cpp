#include "../include/store.hpp"

#include <algorithm>

#include "../include/trust.hpp"

namespace publisher::store {

    namespace {

        std::string IdemKey(const std::string& client_id, const std::string& idempotency_key) {
            return client_id + '\0' + idempotency_key;
        }

    }  // namespace

    void Store::RegisterClient(const Client& c) {
        std::lock_guard<std::mutex> lock(mu_);
        clients_[c.id] = c;
    }

    std::optional<Client> Store::GetClient(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = clients_.find(id);

        if (it == clients_.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    std::vector<Client> Store::AllClients() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<Client> out;
        out.reserve(clients_.size());

        for (const auto& [id, c] : clients_) {
            out.push_back(c);
        }

        std::sort(out.begin(), out.end(),
                  [](const Client& a, const Client& b) { return a.id < b.id; });

        return out;
    }

    void Store::MarkHandshake(const std::string& id) {
        std::lock_guard<std::mutex> lock(mu_);
        handshaken_[id] = true;
    }

    bool Store::IsHandshaken(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = handshaken_.find(id);
        return it != handshaken_.end() && it->second;
    }

    std::optional<Receipt> Store::FindIdempotent(const std::string& client_id,
                                                 const std::string& idempotency_key) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = idempotency_.find(IdemKey(client_id, idempotency_key));
        if (it == idempotency_.end()) return std::nullopt;
        return it->second;
    }

    void Store::SaveIdempotent(const std::string& client_id, const std::string& idempotency_key,
                               const Receipt& r) {
        std::lock_guard<std::mutex> lock(mu_);
        idempotency_[IdemKey(client_id, idempotency_key)] = r;
    }

    void Store::IncAccepted(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(mu_);
        accepted_count_[client_id]++;
    }

    int Store::AcceptedCount(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = accepted_count_.find(client_id);

        return it == accepted_count_.end() ? 0 : it->second;
    }

    Story& Store::StoryRefLocked(const StoryId& id) {
        auto composite = id.Composite();
        auto it = stories_.find(composite);

        if (it == stories_.end()) {
            Story fresh;
            fresh.id = id;
            it = stories_.emplace(composite, std::move(fresh)).first;
        }

        return it->second;
    }

    Story Store::StorySnapshot(const StoryId& id) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = stories_.find(id.Composite());

        if (it == stories_.end()) {
            Story empty;
            empty.id = id;
            return empty;
        }

        return it->second;
    }

    namespace {
        std::optional<int> RankOf(const Story& st, const std::string& client_id) {
            for (const auto& c : st.contributions) {
                if (c.client_id == client_id) {
                    return c.rank;
                }
            }

            return std::nullopt;
        }
    }  // namespace

    SubmitResult Store::SubmitContribution(const StoryId& id, const std::string& client_id,
                                           TimePoint now, std::chrono::duration<double> half_life,
                                           double threshold) {
        std::lock_guard<std::mutex> lock(mu_);
        Story& st = StoryRefLocked(id);

        if (st.published) {
            return SubmitResult{st, 0, false, false, 0.0};
        }

        int rank = 0;
        bool credited = false;

        if (auto existing = RankOf(st, client_id)) {
            rank = *existing;
        } else {
            rank = static_cast<int>(st.contributions.size()) + 1;
            st.contributions.push_back(Contribution{client_id, now, rank});
            credited = true;
        }

        double sum = 0.0;

        for (const auto& c : st.contributions) {
            double weight = 0.0;

            if (auto it = clients_.find(c.client_id); it != clients_.end()) {
                weight = it->second.trust;
            }

            sum += trust::Decayed(weight, now - c.receiving_time, half_life);
        }

        bool published_now = false;

        if (sum >= threshold) {
            st.published = true;
            st.published_at = now;
            st.publish_order = st.contributions;
            published_now = true;
        }

        return SubmitResult{st, rank, credited, published_now, sum};
    }

    std::vector<Story> Store::AllStories() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<Story> out;
        out.reserve(stories_.size());

        for (const auto& [k, st] : stories_) {
            out.push_back(st);
        }

        return out;
    }

    std::vector<Story> Store::AllPublishedStories() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<Story> out;

        for (const auto& [k, st] : stories_) {
            if (st.published) out.push_back(st);
        }

        return out;
    }

}  // namespace publisher::store