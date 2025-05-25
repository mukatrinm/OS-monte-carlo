#include "server.h"
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>

const int DEFAULT_PORT = 12345;

Server::TcpServer *g_tcp_server_instance = nullptr;

void signal_handler_stop_server(int signum) {
    std::cout << "\nSignal " << signum << " received. Initiating server shutdown..." << std::endl;
    if (g_tcp_server_instance) {
        g_tcp_server_instance->shutdown();
    }
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
        return 1;
    }
    if (argc == 2) {
        try {
            port = std::stoi(argv[1]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Error: Port number must be between 1 and 65535." << std::endl;
                return 1;
            }
        } catch (const std::invalid_argument &e) {
            std::cerr << "Error: Invalid port number '" << argv[1] << "'. Must be an integer." << std::endl;
            return 1;
        } catch (const std::out_of_range &e) {
            std::cerr << "Error: Port number '" << argv[1] << "' out of range." << std::endl;
            return 1;
        }
    }

    
    struct sigaction sa;
    sa.sa_handler = signal_handler_stop_server;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting SIGINT handler");
        return 1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Error setting SIGTERM handler");
        return 1;
    }

    try {
        Server::TcpServer server(port);
        g_tcp_server_instance = &server;

        server.start();
        g_tcp_server_instance = nullptr; // when server is stopped
        std::cout << "Main: Server has shut down gracefully." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Server runtime error: " << e.what() << std::endl;
        g_tcp_server_instance = nullptr;
        return 1;
    }

    return 0;
}