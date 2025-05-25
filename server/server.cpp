#include "server.h"
#include "common/ellipse.h"

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace Server {

    TcpServer::TcpServer(int port) : port_{port}, server_socket_fd_{-1} {}

    void TcpServer::start() {
        server_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_fd_ < 0) {
            throw std::runtime_error("Error: Could not create socket. " + std::string(strerror(errno)));
        }

        int opt = 1;
        if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Warning: setsockopt(SO_REUSEADDR) failed. " << strerror(errno) << std::endl;
        }

        sockaddr_in server_address{};
        std::memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(port_);

        if (bind(server_socket_fd_, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            close(server_socket_fd_);
            throw std::runtime_error("Error: Could not bind to port " + std::to_string(port_) + ". " + std::string(strerror(errno)));
        }

        if (listen(server_socket_fd_, 10) < 0) {
            close(server_socket_fd_);
            throw std::runtime_error("Error: Listen failed. " + std::string(strerror(errno)));
        }

        std::cout << "Server listening on port " << port_ << std::endl;

        reactor_.addFd(server_socket_fd_, [this](int fd) {
            accept_handler(fd);
        });

        reactor_.start();
        reactor_.wait();
    }

    void TcpServer::shutdown() {
        std::cout << "TcpServer: Initiating shutdown..." << std::endl;
        reactor_.stop();
    }

    void TcpServer::accept_handler(int server_socket_fd_) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_socket_fd = accept(server_socket_fd_, (struct sockaddr *)&client_address, &client_len);

        if (client_socket_fd < 0) {
            perror("Server::accept_handler accept");

            return;
        }

        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        std::cout << "Connection accepted from " << client_ip_str << ":" << ntohs(client_address.sin_port)
                  << " on new fd " << client_socket_fd << std::endl;

        // Setup per-client session
        ClientSession new_session;
        std::ostringstream fname;
        fname << "client_" << next_log_id_.fetch_add(1) << ".log";
        new_session.log.open(fname.str(), std::ios::out);
        if (!new_session.log.is_open()) {
            std::cerr << "Error: Could not open log file " << fname.str() << std::endl;
        } else {
            new_session.log << std::fixed << std::setprecision(2);
            new_session.log << "Log started for client fd " << client_socket_fd << " from " << client_ip_str << std::endl;
        }

        clients_.emplace(client_socket_fd, std::move(new_session));

        // Add the new client socket to the reactor
        reactor_.addFd(client_socket_fd, [this](int fd) {
            this->client_handler(fd);
        });
    }

    void TcpServer::client_handler(int client_fd) {
        auto it = clients_.find(client_fd);
        if (it == clients_.end()) {
            std::cerr << "Error: No session found for client fd " << client_fd << ". Removing from reactor." << std::endl;
            reactor_.removeFd(client_fd);
            return;
        }

        ClientSession &sess = it->second;

        constexpr size_t RECV_BUF_SIZE = 256;
        char temp_recv_buf[RECV_BUF_SIZE];

        ssize_t nbytes = recv(client_fd, temp_recv_buf, RECV_BUF_SIZE - 1, 0);

        if (nbytes > 0) {
            // temp_recv_buf[nbytes] = '\0'; // For logging
            // std::cout << "Server RX (fd " << client_fd << ", " << nbytes << " bytes): " << temp_recv_buf << std::endl;
            sess.recv_buf.append(temp_recv_buf, static_cast<size_t>(nbytes));

            // Process all complete lines from the session's accumulated buffer
            while (true) {
                auto line_opt = read_line_from_buffer(sess);
                if (!line_opt) {
                    break;
                }

                std::string line = *line_opt;
                // std::cout << "Server Processed Line (fd " << client_fd << "): " << line << std::endl;

                Ellipse ellipse;
                std::istringstream iss(line);
                if (!(iss >> ellipse.cx >> ellipse.cy >> ellipse.a >> ellipse.b) || ellipse.a <= 0 || ellipse.b <= 0) {
                    std::cerr << "Warning: Bad ellipse data from client " << client_fd << ": \"" << line << "\"" << std::endl;
                    if (sess.log.is_open()) {
                        sess.log << "Warning: Received bad ellipse data: " << line << std::endl;
                    }
                    continue;
                }

                sess.simulator.add_ellipse(ellipse);
                MonteCarloResult res = sess.simulator.estimate_area();

                sess.msg_count++;
                if (sess.log.is_open()) {
                    sess.log << "Msg #" << sess.msg_count
                             << ": Ellipses=" << sess.simulator.get_ellipse_count()
                             << ", Est. Area=" << res.covered_area
                             << ", Est. Coverage=" << res.percentage_covered << "%\n";
                    sess.log.flush();
                }

                if (!send_response_to_client(client_fd, res)) {
                    std::cerr << "Error: Failed to send response to client " << client_fd << ". Closing connection." << std::endl;
                    if (sess.log.is_open()) {
                        sess.log << "Error: Failed to send response. Closing connection." << std::endl;
                        sess.log.close();
                    }
                    if (close(client_fd) < 0) {
                        perror("Server::client_handler close");
                    }
                    clients_.erase(it);
                    reactor_.removeFd(client_fd);
                    return;
                }
            }
        } else if (nbytes <= 0) { // Connection closed by peer
            if (nbytes == 0) {
                std::cout << "Client fd " << client_fd << " disconnected." << std::endl;
                if (sess.log.is_open()) {
                    sess.log << "Client disconnected. Total messages: " << sess.msg_count << std::endl;
                    sess.log.close();
                }
            } else { // recv error (nbytes < 0)
                if (sess.log.is_open()) {
                    sess.log << "Recv error. Closing connection. Total messages: " << sess.msg_count << std::endl;
                    sess.log.close();
                }
            }

            if (close(client_fd) < 0) {
                perror("Server::client_handler close");
            }
            clients_.erase(it);
            reactor_.removeFd(client_fd);
        }
    }

    std::optional<std::string> TcpServer::read_line_from_buffer(ClientSession &sess) {
        auto nl_pos = std::find(sess.recv_buf.begin(), sess.recv_buf.end(), '\n');
        if (nl_pos != sess.recv_buf.end()) {
            std::string line(sess.recv_buf.begin(), nl_pos);
            sess.recv_buf.erase(sess.recv_buf.begin(), nl_pos + 1);
            return line;
        }
        return std::nullopt; // No complete data yet
    }

    bool TcpServer::send_response_to_client(int client_fd, const MonteCarloResult &result) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "Covered Area: " << result.covered_area << " units²\n";
        oss << "Percentage of Canvas Covered: " << result.percentage_covered << "%\n";

        std::string response_str = oss.str();
        // std::cout << "Server TX (fd " << client_fd << "):\n" << response_str;
        return send_all(client_fd, response_str.c_str(), response_str.length());
    }

    bool TcpServer::send_all(int sockfd, const char *buffer, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            size_t sent_this_call = send(sockfd, buffer + total_sent, length - total_sent, 0);

            if (sent_this_call <= 0) {
                if (sent_this_call < 0 && errno == EINTR) {
                    continue; // Interrupted by signal, try again
                }
                perror((std::string("Server::send_all failed on fd ") + std::to_string(sockfd)).c_str());
                return false;
            }
            total_sent += sent_this_call;
        }
        return true;
    }
} // namespace Server