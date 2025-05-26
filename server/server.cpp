#include "server.h"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace Server {

    TcpServer::TcpServer(int port) : port_{port}, server_socket_fd_{-1}, is_server_running_{false} {
        std::string log_filename = "server_shared.log";
        shared_log_file_.open(log_filename, std::ios::out);
        if (!shared_log_file_.is_open()) {
            std::cerr << "Warning: Could not open shared log file: " << log_filename << std::endl;
        } else {
            shared_log_file_ << std::fixed << std::setprecision(3);
            shared_log_file_ << "--- Server Started --- Port: " << port_ << std::endl;
        }
    }

    TcpServer::~TcpServer() {
        std::cout << "TcpServer: Destructor called. Ensuring shutdown." << std::endl;
        if (is_server_running_.load()) {
            shutdown();
        }

        std::cout << "TcpServer: All client threads joined." << std::endl;

        if (server_socket_fd_ != -1) {
            close(server_socket_fd_);
            server_socket_fd_ = -1;
        }
        if (shared_log_file_.is_open()) {
            shared_log_file_ << "--- Server Stopped ---" << std::endl;
            shared_log_file_.close();
        }
    }

    bool TcpServer::initialize_listener_socket() {
        server_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_fd_ < 0) {
            perror("Server: socket creation failed");
            return false;
        }

        int opt = 1;
        if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("Server: setsockopt(SO_REUSEADDR) failed");
            return false;
        }

        sockaddr_in server_address{};
        std::memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(server_socket_fd_, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            perror((std::string("Server: bind to port ") + std::to_string(port_) + " failed").c_str());
            close(server_socket_fd_);
            server_socket_fd_ = -1;
            return false;
        }

        if (listen(server_socket_fd_, 10) < 0) {
            perror("Server: listen failed");
            close(server_socket_fd_);
            server_socket_fd_ = -1;
            return false;
        }
        std::cout << "Server listening on port " << port_ << std::endl;
        return true;
    }

    void TcpServer::start() {
        if (!initialize_listener_socket()) {
            return;
        }

        is_server_running_.store(true);
        std::cout << "TcpServer::start: Server is running. Accepting connections." << std::endl;

        while (is_server_running_.load()) {
            sockaddr_in client_address{};
            socklen_t client_len = sizeof(client_address);

            int client_socket_fd = accept(server_socket_fd_, (struct sockaddr *)&client_address, &client_len);

            if (!is_server_running_.load()) {
                if (client_socket_fd >= 0)
                    close(client_socket_fd); // Close any accepted socket if shutting down
                break;
            }

            if (client_socket_fd == -1) {
                if (errno == EINTR)
                    continue; // Interrupted by signal, try again

                perror("Server: accept failed");

                is_server_running_.store(false);
                break;
            }

            char client_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_address.sin_addr, client_ip_str, INET_ADDRSTRLEN);
            int client_id = next_client_id_.fetch_add(1);
            std::cout << "Server: Connection accepted from " << client_ip_str
                      << ":" << ntohs(client_address.sin_port) << " on fd " << client_socket_fd
                      << " (Client ID: " << client_id << ")" << std::endl;

            // Create and detach a new thread for this client.
            std::thread client_thread(&TcpServer::client_handler_thread, this, client_socket_fd, client_id);
            client_thread.detach();
        }

        std::cout << "TcpServer::start: Accept loop finished." << std::endl;
    }

    void TcpServer::shutdown() {
        std::cout << "TcpServer: Initiating shutdown..." << std::endl;
        bool expected_running = true;
        // set is_server_running_ to false. If it was already false, do nothing.
        if (!is_server_running_.compare_exchange_strong(expected_running, false)) {
            std::cout << "TcpServer: Already shutting down or stopped." << std::endl;
            return;
        }

        // Close the listening socket to unblock the accept() call in start()
        if (server_socket_fd_ != -1) {
            std::cout << "TcpServer: Closing listener socket fd " << server_socket_fd_ << std::endl;

            ::shutdown(server_socket_fd_, SHUT_RDWR); // Shutdown the socket for both reading and writing

            if (close(server_socket_fd_) == -1) {
                perror("TcpServer: close() on listener socket failed");
            }
            server_socket_fd_ = -1;
        }

        std::cout << "TcpServer: Shutdown signaled. Main accept loop should terminate. Client threads will exit when is_server_running_ is false." << std::endl;
    }

    void TcpServer::client_handler_thread(int client_socket_fd, int client_id_for_log) {
        std::cout << "Thread for Client ID " << client_id_for_log << " (fd: " << client_socket_fd << ") started." << std::endl;
        std::string client_recv_buf; // Buffer for this client's partial messages
        int message_count_this_client = 0;
        MonteCarloSimulator thread_local_simulator; // Each thread gets its own simulator

        while (is_server_running_.load()) {
            ReadLineResult read_result = read_line_from_socket(client_socket_fd, client_recv_buf);

            if (read_result.status == ReadLineStatus::Success) {
                std::string line = *read_result.line;
                Ellipse e;
                std::istringstream iss(line);
                if (!(iss >> e.cx >> e.cy >> e.a >> e.b) || e.a <= 0 || e.b <= 0) {
                    std::cerr << "Thread Client ID " << client_id_for_log << ": Bad ellipse data: \"" << line << "\"" << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(shared_resources_mutex_);
                        if (shared_log_file_.is_open()) {
                            shared_log_file_ << "[Client " << client_id_for_log << "/fd " << client_socket_fd
                                             << "] Warning: Bad ellipse data: " << line << std::endl;
                        }
                    }
                    continue;
                }

                size_t num_ellipses_in_shared_list = 0;
                {
                    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
                    shared_ellipses_.push_back(e);
                    thread_local_simulator.add_ellipse(e);
                    num_ellipses_in_shared_list = shared_ellipses_.size();
                }

                MonteCarloResult sim_res = thread_local_simulator.estimate_area();
                message_count_this_client++;

                if (!send_response_to_client(client_socket_fd, sim_res)) {
                    std::cerr << "Thread Client ID " << client_id_for_log << ": Send failed. Closing." << std::endl;
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
                    messages_processed_count_++;
                    if (shared_log_file_.is_open()) {
                        shared_log_file_ << "[Client " << client_id_for_log << "/fd " << client_socket_fd << "] "
                                         << "Msg #" << message_count_this_client
                                         << " (Server Total Msgs: " << messages_processed_count_ << ")"
                                         << ": Ellipses in shared list=" << num_ellipses_in_shared_list
                                         << " (Client local sim count=" << thread_local_simulator.get_ellipse_count() << ")"
                                         << ", Est. Area=" << sim_res.covered_area
                                         << ", Est. Coverage=" << sim_res.percentage_covered << "%\n";
                        shared_log_file_.flush();
                    }
                }

            } else if (read_result.status == ReadLineStatus::Disconnected) {
                std::cout << "Thread for Client ID " << client_id_for_log << " (fd: " << client_socket_fd << "): Client disconnected." << std::endl;
                {
                    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
                    if (shared_log_file_.is_open()) {
                        shared_log_file_ << "[Client " << client_id_for_log << "/fd " << client_socket_fd
                                         << "] Client disconnected. Messages processed by this client: " << message_count_this_client << std::endl;
                    }
                }
                break;
            } else if (read_result.status == ReadLineStatus::Error) {
                if (is_server_running_.load()) { // Avoid redundant logging if already logged by read_line
                    std::cerr << "Thread for Client ID " << client_id_for_log << " (fd: " << client_socket_fd << "): Read error occurred." << std::endl;
                }
                {
                    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
                    if (shared_log_file_.is_open()) {
                        shared_log_file_ << "[Client " << client_id_for_log << "/fd " << client_socket_fd
                                         << "] Read error. Messages processed by this client: " << message_count_this_client << std::endl;
                    }
                }
                break; // Exit thread loop
            } else if (read_result.status == ReadLineStatus::ServerShutdown) {
                std::cout << "Thread for Client ID " << client_id_for_log << " (fd: " << client_socket_fd << "): Server shutting down, exiting read loop." << std::endl;
                break;
            }

            if (!is_server_running_.load()) {
                break;
            }
        }

        // Cleanup for this client connection
        close(client_socket_fd);
        std::cout << "Thread for Client ID " << client_id_for_log << " (fd: " << client_socket_fd << ") finished." << std::endl;
    }

    TcpServer::ReadLineResult TcpServer::read_line_from_socket(int client_socket_fd, std::string &client_recv_buf) {
        char buffer[256];

        while (true) {
            if (!is_server_running_.load()) {
                return {ReadLineStatus::ServerShutdown, std::nullopt};
            }

            // Try to find newline in existing buffer first
            auto nl_pos = client_recv_buf.find('\n');
            if (nl_pos != std::string::npos) {
                std::string line_content = client_recv_buf.substr(0, nl_pos);
                client_recv_buf.erase(0, nl_pos + 1);
                return {ReadLineStatus::Success, line_content};
            }

            // If no newline, read more data
            ssize_t nbytes = recv(client_socket_fd, buffer, sizeof(buffer) - 1, 0);

            if (nbytes > 0) {
                buffer[nbytes] = '\0';
                client_recv_buf.append(buffer, nbytes);
                // After appending, loop again to check for newline
                continue;
            } else if (nbytes == 0) {
                return {ReadLineStatus::Disconnected, std::nullopt};
            } else { // Error (nbytes < 0)
                if (errno == EINTR) {
                    // Interrupted by signal, loop again to check is_server_running_ and retry recv
                    continue;
                }

                return {ReadLineStatus::Error, std::nullopt};
            }
        }
    }

    bool TcpServer::send_response_to_client(int client_fd, const MonteCarloResult &result) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "Covered Area: " << result.covered_area << " units²\n";
        oss << "Percentage of Canvas Covered: " << result.percentage_covered << "%\n";
        std::string response_str = oss.str();
        return send_all(client_fd, response_str.c_str(), response_str.length());
    }

    bool TcpServer::send_all(int sockfd, const char *buffer, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            if (!is_server_running_.load())
                return false; // Stop sending if server is shutting down
            ssize_t sent_this_call = send(sockfd, buffer + total_sent, length - total_sent, 0);
            if (sent_this_call <= 0) {
                if (sent_this_call < 0 && errno == EINTR)
                    continue;
                if (is_server_running_.load()) { // Don't print error if due to shutdown
                    perror((std::string("Server::send_all failed on fd ") + std::to_string(sockfd)).c_str());
                }
                return false;
            }
            total_sent += static_cast<size_t>(sent_this_call);
        }
        return true;
    }

} // namespace Server