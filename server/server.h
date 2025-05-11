#pragma once

#include "monte_carlo_simulator.h"
#include <optional>
#include <string>

namespace Server {

    /**
     * @brief Manages the server-side operations including network communication and simulation.
     */
    class TcpServer {
    public:
        /**
         * @brief Constructs the server.
         * @param port The port number to listen on.
         */
        TcpServer(int port);

        /**
         * @brief Starts the server and begins listening for client connections.
         * This function will run indefinitely, handling one client at a time.
         */
        void start();

    private:
        /**
         * @brief Handles communication with a single connected client.
         * @param client_socket_fd The file descriptor for the client's socket.
         */
        void handle_client(int client_socket_fd);

        /**
         * @brief Reads a line of text from the client socket.
         * @param client_socket_fd The client socket file descriptor.
         * @param success Reference to a boolean flag, set to false on read error or disconnect.
         * @return The line read (without newline), or std::nullopt on error/disconnect.
         */
        std::optional<std::string> read_line_from_client(int client_socket_fd, bool &success);

        /**
         * @brief Sends the simulation result back to the client.
         * @param client_socket_fd The client socket file descriptor.
         * @param result The Monte Carlo simulation result.
         * @return True if sending was successful, false otherwise.
         */
        bool send_response_to_client(int client_socket_fd, const MonteCarloResult &result);

        /**
         * @brief Sends a complete message buffer over the socket.
         * Handles potential partial sends.
         * @param sockfd The socket file descriptor.
         * @param buffer The data buffer to send.
         * @param length The length of the data in the buffer.
         * @return True on success, false on failure.
         */
        bool send_all(int sockfd, const char *buffer, size_t length);

        int port_;
        int server_socket_fd_;
        MonteCarloSimulator simulator_;
        std::string recv_buf_;
    };

} // namespace Server