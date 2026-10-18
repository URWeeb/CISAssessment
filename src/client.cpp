#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../include/protocol.hpp"

using publisher::protocol::JoinTab;
using publisher::protocol::SplitTab;

namespace {

    int ConnectTo(const std::string& host, const std::string& port) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;

        if (int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res); rc != 0) {
            std::cerr << "getaddrinfo: " << gai_strerror(rc) << "\n";
            return -1;
        }

        int fd = -1;
        
        for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
            fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            
            if (fd < 0) {
                continue;
            }

            if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
                break;
            }

            ::close(fd);
            fd = -1;
        }
        ::freeaddrinfo(res);
        return fd;
    }

    std::string ReadLine(int fd) {
        std::string line;
        char ch;

        while (true) {
            ssize_t n = ::recv(fd, &ch, 1, 0);

            if (n <= 0) {
                break;
            }

            if (ch == '\n') {
                break;
            }

            line.push_back(ch);
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        return line;
    }

    void WriteLine(int fd, const std::string& line) {
        std::string with_nl = line + "\n";
        size_t sent = 0;

        while (sent < with_nl.size()) {
            ssize_t n = ::send(fd, with_nl.data() + sent, with_nl.size() - sent, 0);

            if (n <= 0) {
                return;
            }

            sent += static_cast<size_t>(n);
        }
    }

    std::pair<int, std::string> SendRequest(const std::string& host, const std::string& port,
                                            const std::string& request_line) {
        int fd = ConnectTo(host, port);

        if (fd < 0) {
            return {-1, ""};
        }

        WriteLine(fd, request_line);
        std::string reply = ReadLine(fd);

        return {fd, reply};
    }

    void PrintUsage() {
        std::cerr <<
            "Использование:\n"
            "  izdanie_client <host> <port> hello <client_id>\n"
            "  izdanie_client <host> <port> note  <client_id> <idempotency_key> <category> <topic...>\n"
            "  izdanie_client <host> <port> stats <client_id>\n"
            "  izdanie_client <host> <port> stats-all <client_id>\n"
            "  izdanie_client <host> <port> poll  <client_id> <since_seq>\n";
    }

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() < 3) {
        PrintUsage();
        return 1;
    }
    const std::string host = args[0];
    const std::string port = args[1];
    const std::string cmd = args[2];

    if (cmd == "hello" && args.size() >= 4) {
        auto [fd, reply] = SendRequest(host, port, JoinTab({"HELLO", args[3]}));
        
        if (fd >= 0) {
            ::close(fd);
        }
        
        std::cout << reply << "\n";
    } else if (cmd == "note" && args.size() >= 7) {
        std::string topic = args[6];

        for (size_t i = 7; i < args.size(); ++i) {
            topic += " " + args[i];
        }

        auto [fd, reply] = SendRequest(host, port, JoinTab({"NOTE", args[3], args[4], args[5], topic}));

        if (fd >= 0) {
            ::close(fd);
        }

        std::cout << reply << "\n";
    } else if (cmd == "stats" && args.size() >= 4) {
        auto [fd, reply] = SendRequest(host, port, JoinTab({"STATS", args[3]}));
        
        if (fd >= 0) {
            ::close(fd);
        }

        std::cout << reply << "\n";
    } else if (cmd == "stats-all" && args.size() >= 4) {
        int fd = ConnectTo(host, port);

        if (fd < 0) {
            std::cerr << "не удалось подключиться\n"; 
            return 1;
        }

        WriteLine(fd, JoinTab({"STATS_ALL", args[3]}));
        std::string header = ReadLine(fd);
        std::cout << header << "\n";
        auto fields = SplitTab(header);

        if (fields.size() >= 2 && fields[0] == "OK") {
            size_t n = std::stoul(fields[1]);

            for (size_t i = 0; i < n; ++i) {
                std::cout << ReadLine(fd) << "\n";
            }
        }

        ::close(fd);
    } else if (cmd == "poll" && args.size() >= 5) {
        int fd = ConnectTo(host, port);
        
        if (fd < 0) {
            std::cerr << "не удалось подключиться\n"; return 1;
        }

        WriteLine(fd, JoinTab({"POLL", args[3], args[4]}));
        std::string header = ReadLine(fd);
        std::cout << header << "\n";
        auto fields = SplitTab(header);

        if (fields.size() >= 2 && fields[0] == "OK") {
            size_t n = std::stoul(fields[1]);

            for (size_t i = 0; i < n; ++i) {
                std::cout << ReadLine(fd) << "\n";
            }
        }

        ::close(fd);
    } else {
        PrintUsage();
        return 1;
    }

    return 0;
}