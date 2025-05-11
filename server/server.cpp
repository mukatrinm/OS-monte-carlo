#include "server.h"
#include "common/ellipse.h"
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Server {

    TcpServer::TcpServer(int port) : port_(port), server_socket_fd_(-1) {
        simulator_.clear_ellipses();
    }

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
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(port_);

        if (bind(server_socket_fd_, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            close(server_socket_fd_);
            throw std::runtime_error("Error: Could not bind to port " + std::to_string(port_) + ". " + std::string(strerror(errno)));
        }

        if (listen(server_socket_fd_, 5) < 0) {
            close(server_socket_fd_);
            throw std::runtime_error("Error: Listen failed. " + std::string(strerror(errno)));
        }

        std::cout << "Server listening on port " << port_ << std::endl;

        while (true) {
            sockaddr_in client_address{};
            socklen_t client_len = sizeof(client_address);
            int client_socket_fd = accept(server_socket_fd_, (struct sockaddr *)&client_address, &client_len);

            if (client_socket_fd < 0) {
                std::cerr << "Error: Accept failed. " << strerror(errno) << ". Continuing..." << std::endl;
                continue;
            }

            char client_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_address.sin_addr, client_ip_str, INET_ADDRSTRLEN);
            std::cout << "Connection accepted from " << client_ip_str << ":" << ntohs(client_address.sin_port) << std::endl;

            simulator_.clear_ellipses(); // Reset simulator for new client session
            handle_client(client_socket_fd);
            close(client_socket_fd);
            std::cout << "Connection closed with " << client_ip_str << std::endl;
        }
    }

    void TcpServer::handle_client(int client_socket_fd) {
        bool client_connected = true;
        while (client_connected) {
            auto line_opt = read_line_from_client(client_socket_fd, client_connected);

            if (!client_connected || !line_opt) {
                break;
            }

            std::string line = *line_opt;
            std::cout << "Server RX: " << line << std::endl;

            std::istringstream iss(line);
            Ellipse ellipse;
            if (!(iss >> ellipse.cx >> ellipse.cy >> ellipse.a >> ellipse.b)) {
                std::cerr << "Error: Could not parse ellipse data from client: " << line << std::endl;
                break;
            }

            if (ellipse.a <= 0 || ellipse.b <= 0) {
                std::cerr << "Error: Invalid ellipse parameters (a or b not positive): a="
                          << ellipse.a << ", b=" << ellipse.b << std::endl;
                break;
            }

            simulator_.add_ellipse(ellipse);
            std::cout << "Added ellipse. Total ellipses: " << simulator_.get_ellipse_count() << std::endl;

            MonteCarloResult result = simulator_.estimate_area();

            if (!send_response_to_client(client_socket_fd, result)) {
                std::cerr << "Error: Failed to send response to client." << std::endl;
                break;
            }
        }
    }

    std::optional<std::string> TcpServer::read_line_from_client(int client_socket_fd, bool &success) {
        constexpr std::size_t BUF_SIZE = 256;
        char temp[BUF_SIZE];
        success = true;

        while (true) {
            // First check if we already have a full line in recv_buf_
            auto nl = std::find(recv_buf_.begin(), recv_buf_.end(), '\n');
            if (nl != recv_buf_.end()) {
                std::string line(recv_buf_.begin(), nl);
                recv_buf_.erase(recv_buf_.begin(), nl + 1);
                return line;
            }

            // More data needed from client
            ssize_t n = recv(client_socket_fd, temp, BUF_SIZE, 0);
            if (n > 0) {
                recv_buf_.append(temp, static_cast<std::size_t>(n));
            } else if (n == 0) { // peer closed connection
                success = false;
                return std::nullopt;
            } else { // error
                if (errno == EINTR)
                    continue;
                perror("recv error in read_line_from_client");
                success = false;
                return std::nullopt;
            }
        }
    }

    bool TcpServer::send_all(int sockfd, const char *buffer, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            ssize_t sent_this_call = send(sockfd, buffer + total_sent, length - total_sent, 0);
            if (sent_this_call < 0) {
                if (errno == EINTR)
                    continue; // Interrupted by signal, try again
                perror("send_all failed");
                return false;
            }

            total_sent += sent_this_call;
        }
        return true;
    }

    bool TcpServer::send_response_to_client(int client_socket_fd, const MonteCarloResult &result) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2); // Format numbers to two decimal places
        oss << "Covered Area: " << result.covered_area << " units²\n";
        oss << "Percentage of Canvas Covered: " << result.percentage_covered << "%\n";

        std::string response_str = oss.str();
        std::cout << "Server TX:\n"
                  << response_str;
        return send_all(client_socket_fd, response_str.c_str(), response_str.length());
    }

} // namespace Server