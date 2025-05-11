#pragma once

#include "common/ellipse.h"
#include "ellipse_generator.h"
#include <string>

namespace Client {

    /**
     * @brief Manages client-side operations including connecting to server and sending ellipses.
     */
    class TcpClient {
    public:
        /**
         * @brief Constructor.
         * @param host The server hostname or IP address.
         * @param port The server port number.
         */
        TcpClient(const std::string &host, int port);

        /**
         * @brief Destructor. Closes connection if open.
         */
        ~TcpClient();

        /**
         * @brief Connects to the server.
         * @return True if connection is successful, false otherwise.
         */
        bool connect_to_server();

        /**
         * @brief Sends an ellipse to the server and waits for a response.
         * @param ellipse The ellipse to send.
         * @return True if the send/receive cycle was successful, false otherwise.
         */
        bool send_ellipse_and_get_response(const Ellipse &ellipse);

        /**
         * @brief Closes the connection to the server.
         */
        void disconnect();

    private:
        /**
         * @brief Sends ellipse data to the server.
         * @param ellipse The ellipse to send.
         * @return True on success, false otherwise.
         */
        bool transmit_ellipse_data(const Ellipse &ellipse);

        /**
         * @brief Reads the server's response.
         * Reads two lines: area and percentage.
         * @return True if response read successfully, false otherwise.
         */
        bool receive_server_response();

        /**
         * @brief Reads a line of text from the server socket.
         * @param success Reference to a boolean flag, set to false on read error or disconnect.
         * @return The line read (without newline), or std::nullopt on error/disconnect.
         */
        std::optional<std::string> read_line_from_server(bool &success);

        /**
         * @brief Sends a complete message buffer over the socket.
         * @param sockfd The socket file descriptor.
         * @param buffer The data buffer to send.
         * @param length The length of the data in the buffer.
         * @return True on success, false on failure.
         */
        bool send_all(int sockfd, const char *buffer, size_t length);

        std::string host_;
        int port_;
        int socket_fd_;
        std::string recv_buf_;
        bool connected_;
    };

} // namespace Client