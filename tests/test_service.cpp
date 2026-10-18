#include <gtest/gtest.h>

#include "../include/service.hpp"

using namespace std::chrono_literals;
using namespace publisher;

namespace {
    struct FakeClock {
    public:
        TimePoint now = Clock::now();

    public:
        TimePoint operator()() const { return now; }
    };

    struct Harness {
    public:
        store::Store store;
        mailbox::Registry mailbox;
        FakeClock clock;
        service::Service svc;

    public:
        explicit Harness(service::Config cfg)
            : svc(store, mailbox, cfg, [this]() { return clock(); }) {}
    };

}  // namespace

TEST(Service, HandshakeRequiredBeforeNotes) {
    Harness h(service::Config{});
    h.svc.RegisterClient(Client{"alice", "", ClientLevel::FREELANCER, 1.0, false});
    Note note;
    note.client_id = "alice";
    note.idempotency_key = "k1";
    note.id = StoryId{"двор", "тема"};
    bool threw = false;

    try {
        h.svc.SubmitNote(note);
    } catch (const service::ServiceError& e) {
        threw = (e.code == service::ErrorCode::HANDSHAKE_REQUIRED);
    }

    EXPECT_TRUE(threw);
}

TEST(Service, IdempotentRetryReturnsDuplicate) {
    Harness h(service::Config{});
    h.svc.RegisterClient(Client{"alice", "", ClientLevel::FREELANCER, 1.0, false});
    h.svc.Handshake("alice");

    Note note;
    note.client_id = "alice";
    note.idempotency_key = "k1";
    note.id = StoryId{"двор", "тема"};

    auto r1 = h.svc.SubmitNote(note);
    EXPECT_EQ(r1.status, ReceiptStatus::ACCEPTED);

    auto r2 = h.svc.SubmitNote(note);
    EXPECT_EQ(r2.status, ReceiptStatus::DUPLICATE);
    EXPECT_EQ(r2.rank, r1.rank);
}

TEST(Service, ThresholdReachedPublishesAndNotifies) {
    service::Config cfg;
    cfg.threshold = 0.75;
    Harness h(cfg);
    h.svc.RegisterClient(Client{"alice", "", ClientLevel::FREELANCER, 1.0, false});
    h.svc.Handshake("alice");

    Note note;
    note.client_id = "alice";
    note.idempotency_key = "k1";
    note.id = StoryId{"двор", "тема"};

    auto r = h.svc.SubmitNote(note);
    EXPECT_TRUE(r.story_published_initiator);

    auto events = h.svc.Events("alice", 0);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, ServerEventType::CONGRATULATION);
}

TEST(Service, StatsAllRequiresEditor) {
    Harness h(service::Config{});
    h.svc.RegisterClient(Client{"alice", "", ClientLevel::FREELANCER, 1.0, false});
    h.svc.RegisterClient(Client{"boss", "", ClientLevel::STAFF, 0.0, true});

    bool threw = false;
    try {
        h.svc.AllStats("alice");
    } catch (const service::ServiceError& e) {
        threw = (e.code == service::ErrorCode::FORBIDDEN);
    }
    EXPECT_TRUE(threw);

    auto all = h.svc.AllStats("boss");
    EXPECT_EQ(all.size(), 2u);
}
