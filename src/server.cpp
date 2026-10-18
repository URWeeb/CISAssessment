#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../include/protocol.hpp"
#include "../include/service.hpp"
#include "../include/store.hpp"

using namespace publisher;
using publisher::protocol::JoinTab;
using publisher::protocol::SplitTab;

namespace {

    struct Args {
        int port = 7878;
        double threshold = 0.75;
        double half_life_hours = 6.0;
        int congratulate_top = 3;
        std::string roster;
    };

    std::string ToStr(ReceiptStatus s) {
        switch (s) {
            case ReceiptStatus::ACCEPTED:
                return "ACCEPTED";
            case ReceiptStatus::DUPLICATE:
                return "DUPLICATE";
            case ReceiptStatus::REJECTED:
                return "REJECTED";
        }

        return "REJECTED";
    }

    std::string ReadLine(int fd) {
        std::string line;
        char ch;

        while (true) {
            ssize_t n = ::recv(fd, &ch, 1, 0);
            if (n <= 0) break;
            if (ch == '\n') break;
            line.push_back(ch);
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        return line;
    }

    void WriteLine(int fd, const std::string& line) {
        std::string withNl = line + "\n";
        size_t sent = 0;

        while (sent < withNl.size()) {
            ssize_t n = ::send(fd, withNl.data() + sent, withNl.size() - sent, 0);

            if (n <= 0) {
                return;
            }

            sent += static_cast<size_t>(n);
        }
    }

    void LoadRoster(const std::string& path, service::Service& svc) {
        std::ifstream in(path);

        if (!in) {
            std::cerr << "не удалось открыть файл ростера: " << path << "\n";
            return;
        }

        std::string line;

        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::stringstream ss(line);
            std::string id, name, lvl_str, trust_str, editor_str;
            std::getline(ss, id, ',');
            std::getline(ss, name, ',');
            std::getline(ss, lvl_str, ',');
            std::getline(ss, trust_str, ',');
            std::getline(ss, editor_str, ',');

            if (id.empty()) {
                continue;
            }

            Client c;
            c.id = id;
            c.name = name;
            c.lvl = (lvl_str == "staff") ? ClientLevel::STAFF : ClientLevel::FREELANCER;
            c.trust = trust_str.empty() ? 0.0 : std::stod(trust_str);
            c.is_editor = (editor_str == "1" || editor_str == "true");
            svc.RegisterClient(c);

            std::cerr << "заведён клиент: " << c.id << " (" << c.name << "), доверие=" << c.trust
                      << (c.is_editor ? ", редактор" : "") << "\n";
        }
    }

    void HandleConnection(int client_fd, service::Service& svc) {
        std::string request = ReadLine(client_fd);
        auto fields = SplitTab(request);
        const std::string& cmd = fields.empty() ? std::string() : fields[0];

        try {
            if (cmd == "HELLO" && fields.size() >= 2) {
                svc.Handshake(fields[1]);
                WriteLine(client_fd, JoinTab({"OK"}));
            } else if (cmd == "NOTE" && fields.size() >= 5) {
                Note note;
                note.client_id = fields[1];
                note.idempotency_key = fields[2];
                note.id.category = fields[3];
                note.id.topic = fields[4];
                Receipt r = svc.SubmitNote(note);
                WriteLine(client_fd, JoinTab({ToStr(r.status), r.credited ? "1" : "0",
                                              r.story_published_initiator ? "1" : "0",
                                              std::to_string(r.rank)}));
            } else if (cmd == "STATS" && fields.size() >= 2) {
                ClientStats st = svc.StatsFor(fields[1]);
                WriteLine(
                    client_fd,
                    JoinTab({"OK", std::to_string(st.accepted_counter),
                             std::to_string(st.stories_touched),
                             std::to_string(st.published_counter), std::to_string(st.share),
                             std::to_string(st.sensations), std::to_string(st.average_rank)}));
            } else if (cmd == "STATS_ALL" && fields.size() >= 2) {
                auto all = svc.AllStats(fields[1]);
                WriteLine(client_fd, JoinTab({"OK", std::to_string(all.size())}));

                for (const auto& st : all) {
                    WriteLine(
                        client_fd,
                        JoinTab({st.client_id, std::to_string(st.accepted_counter),
                                 std::to_string(st.stories_touched),
                                 std::to_string(st.published_counter), std::to_string(st.share),
                                 std::to_string(st.sensations), std::to_string(st.average_rank)}));
                }

            } else if (cmd == "POLL" && fields.size() >= 3) {
                std::uint64_t after = std::stoull(fields[2]);
                auto events = svc.Events(fields[1], after);
                WriteLine(client_fd, JoinTab({"OK", std::to_string(events.size())}));

                for (const auto& e : events) {
                    WriteLine(client_fd,
                              JoinTab({std::to_string(e.seq), "congratulation", e.payload}));
                }

            } else {
                WriteLine(client_fd, JoinTab({"ERR", "неизвестная или неполная команда"}));
            }
        } catch (const service::ServiceError& e) {
            WriteLine(client_fd, JoinTab({"ERR", e.what()}));
        } catch (const std::exception& e) {
            WriteLine(client_fd, JoinTab({"ERR", std::string("внутренняя ошибка: ") + e.what()}));
        }

        ::close(client_fd);
    }

    Args ParseArgs(int argc, char** argv) {
        Args a;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            auto value = [&](const std::string& prefix) -> std::optional<std::string> {
                if (arg.rfind(prefix, 0) == 0) {
                    return arg.substr(prefix.size());
                }

                return std::nullopt;
            };

            if (auto v = value("--port=")) {
                a.port = std::stoi(*v);
            } else if (auto v2 = value("--threshold=")) {
                a.threshold = std::stod(*v2);
            } else if (auto v3 = value("--half-life-hours=")) {
                a.half_life_hours = std::stod(*v3);
            } else if (auto v4 = value("--congratulate-top=")) {
                a.congratulate_top = std::stoi(*v4);
            } else if (auto v5 = value("--roster=")) {
                a.roster = *v5;
            }
        }

        return a;
    }

}  // namespace

int main(int argc, char** argv) {
    Args args = ParseArgs(argc, argv);
    store::Store store;
    mailbox::Registry mailbox;
    service::Config cfg;
    cfg.threshold = args.threshold;
    cfg.half_life = std::chrono::duration<double>(args.half_life_hours * 3600.0);
    cfg.congratulate_top = args.congratulate_top;
    service::Service svc(store, mailbox, cfg);

    if (!args.roster.empty()) {
        LoadRoster(args.roster, svc);
    }

    ::signal(SIGPIPE, SIG_IGN);

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }

    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(args.port));

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        return 1;
    }

    if (::listen(listen_fd, 128) < 0) {
        std::perror("listen");
        return 1;
    }

    std::cerr << "Издание слушает на порту " << args.port << " (порог=" << args.threshold
              << ", период полураспада=" << args.half_life_hours
              << "ч, топ для поздравлений=" << args.congratulate_top << ")\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            continue;
        }
        std::thread(HandleConnection, client_fd, std::ref(svc)).detach();
    }
}