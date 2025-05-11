#include "client.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Client {

    TcpClient::TcpClient(const std::string &host, int port)
        : host_(host), port_(port), socket_fd_(-1), connected_(false) {}

    TcpClient::~TcpClient() {
        disconnect();
    }

    bool TcpClient::connect_to_server() {
        if (connected_) {
            std::cerr << "Already connected." << std::endl;
            return true;
        }

        socket_fd_ = -1;
        struct addrinfo hints{}, *result = nullptr, *rp = nullptr;
        std::memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string port_str = std::to_string(port_);
        if (int rv = getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &result) != 0) {
            std::cerr << "Client: Error resolving hostname: " << gai_strerror(rv) << std::endl;
            return false;
        }

        for (rp = result; rp != nullptr; rp = rp->ai_next) {
            socket_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket_fd_ == -1) {
                perror("Client: socket");
                continue;
            }

            if (connect(socket_fd_, rp->ai_addr, rp->ai_addrlen) == 0)
                break; // success

            perror("Client: connect");
            close(socket_fd_);
            socket_fd_ = -1;
        }

        freeaddrinfo(result);

        if (rp == nullptr) { // No address succeeded
            std::cerr << "Client: Failed to connect to server " << host_ << ":" << port_ << std::endl;
            return false;
        }

        std::cout << "Client: Connected to server " << host_ << ":" << port_ << std::endl;
        connected_ = true;
        return true;
    }

    bool TcpClient::send_ellipse_and_get_response(const Ellipse &ellipse) {
        if (!connected_) {
            std::cerr << "Client: Not connected to server." << std::endl;
            return false;
        }
        if (!transmit_ellipse_data(ellipse)) {
            return false;
        }
        return receive_server_response();
    }

    bool TcpClient::transmit_ellipse_data(const Ellipse &ellipse) {
        std::ostringstream oss;
        // Ensure high precision for doubles to avoid truncation
        oss << std::fixed << std::setprecision(10);
        oss << ellipse.cx << " " << ellipse.cy << " " << ellipse.a << " " << ellipse.b << "\n";
        std::string ellipse_str = oss.str();

        std::cout << "Client TX: " << ellipse_str;
        return send_all(socket_fd_, ellipse_str.c_str(), ellipse_str.length());
    }

    bool TcpClient::receive_server_response() {
        bool success = true;
        auto area_line = read_line_from_server(success);
        if (!success || !area_line) {
            std::cerr << "Client: Failed to read area line from server or server disconnected." << std::endl;
            return false;
        }

        auto percentage_line = read_line_from_server(success);
        if (!success || !percentage_line) {
            std::cerr << "Client: Failed to read percentage line from server or server disconnected." << std::endl;
            return false;
        }

        std::cout << "Client RX:\n"
                  << *area_line << "\n"
                  << *percentage_line << std::endl;
        return true;
    }

    std::optional<std::string> TcpClient::read_line_from_server(bool &success) {
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

            // More data needed from server
            ssize_t n = recv(socket_fd_, temp, BUF_SIZE, 0);
            if (n > 0) {
                recv_buf_.append(temp, static_cast<std::size_t>(n));
            } else if (n == 0) { // peer closed connection
                success = false;
                return std::nullopt;
            } else { // error
                if (errno == EINTR)
                    continue;
                perror("Client: recv");
                success = false;
                return std::nullopt;
            }
        }
    }

    bool TcpClient::send_all(int sockfd, const char *buffer, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            ssize_t sent_this_call = send(sockfd, buffer + total_sent, length - total_sent, 0);
            if (sent_this_call < 0) {
                if (errno == EINTR)
                    continue; // Interrupted by signal, try again
                perror("Client: send_all failed");
                connected_ = false;
                return false;
            }

            total_sent += sent_this_call;
        }
        return true;
    }

    void TcpClient::disconnect() {
        if (connected_ && socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
            connected_ = false;
            std::cout << "Client: Disconnected from server." << std::endl;
        }
    }

} // namespace Client