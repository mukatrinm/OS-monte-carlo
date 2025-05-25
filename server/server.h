#pragma once

#include "monte_carlo_simulator.h"
#include "reactor/reactor.h"

#include <atomic>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

namespace Server {

    struct ClientSession {
        MonteCarloSimulator simulator;
        std::string recv_buf;
        std::ofstream log; // per-client log file
        int msg_count{0};
    };

    class TcpServer {
    public:
        explicit TcpServer(int port);
        void start();
        void shutdown();

    private:
        void accept_handler(int listener_fd);
        void client_handler(int client_fd);

        std::optional<std::string> read_line_from_buffer(ClientSession &sess);
        bool send_response_to_client(int client_fd,
                                     const MonteCarloResult &result);
        bool send_all(int sockfd, const char *buffer, size_t length);

        int port_;
        int server_socket_fd_;
        Reactor reactor_;
        std::unordered_map<int, ClientSession> clients_;
        std::atomic<int> next_log_id_{1};
    };

} // namespace Server