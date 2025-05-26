#pragma once

#include "common/ellipse.h"
#include "monte_carlo_simulator.h"

#include <atomic>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Server {
    enum class ReadLineStatus {
        Success,       // A line was successfully read
        Disconnected,  // Client disconnected (EOF)
        Error,         // A read error occurred (e.g., network issue, bad FD)
        ServerShutdown // Reading was aborted because server is shutting down
    };

    struct CoverageEvent {
        double percentage_coverage_achieved;   // The coverage % calculated by a client
        size_t total_server_ellipses_at_event; // Total ellipses in shared_ellipses_ when this event was generated
        int client_id;
    };

    class TcpServer {
    public:
        explicit TcpServer(int port);
        ~TcpServer();

        TcpServer(const TcpServer &) = delete;
        TcpServer &operator=(const TcpServer &) = delete;

        void start();    // Main server loop: accepts connections, spawns threads
        void shutdown(); // Signals server and threads to stop

    private:
        struct ReadLineResult {
            ReadLineStatus status;
            std::optional<std::string> line; // Contains line only if status is Success
        };

        void client_handler_thread(int client_socket_fd, int client_id_for_log);

        ReadLineResult read_line_from_socket(int client_socket_fd, std::string &client_recv_buf);
        bool send_response_to_client(int client_socket_fd, const MonteCarloResult &result);
        bool send_all(int sockfd, const char *buffer, size_t length);

        bool initialize_listener_socket();

        void notifier_thread_function();

        int port_;
        int server_socket_fd_;

        std::atomic<bool> is_server_running_{false}; // Controls main accept loop and signals threads

        // --- Shared Resources ---
        std::vector<Ellipse> shared_ellipses_; // All ellipses from all clients, for debugging and logging
        std::mutex shared_resources_mutex_;    // Mutex to protect shared_ellipses_ and shared_log_file_
        std::ofstream shared_log_file_;        // Single log file for all server activity
        std::atomic<int> next_client_id_{1};   // For unique client IDs in logs/threads
        size_t messages_processed_count_{0};   // Total number of messages processed by the server

        // Resources for Notifier Thread (Producer-Consumer Pattern)
        std::thread notifier_thread_;
        std::deque<CoverageEvent> coverage_event_queue_; // Shared queue for coverage events
        std::mutex queue_mutex_;                         // Mutex for coverage_event_queue_
        std::condition_variable queue_cv_;               // CV for coverage_event_queue_
        std::ofstream milestone_log_file_;
    };

} // namespace Server