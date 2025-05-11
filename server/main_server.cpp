#include "server.h"
#include <iostream>
#include <stdexcept>
#include <string>

const int DEFAULT_PORT = 12345;

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

    try {
        Server::TcpServer server(port);
        server.start();
    } catch (const std::exception &e) {
        std::cerr << "Server runtime error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}