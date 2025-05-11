#include "client.h"
#include "ellipse_generator.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

const int DEFAULT_SERVER_PORT = 12345;
const std::string DEFAULT_SERVER_HOST = "127.0.0.1";
const unsigned int DEFAULT_SEED = 42;
const int DEFAULT_NUM_ELLIPSES = 10;

int main(int argc, char *argv[]) {
    std::string host = DEFAULT_SERVER_HOST;
    int port = DEFAULT_SERVER_PORT;
    unsigned int seed = DEFAULT_SEED;
    int num_ellipses = DEFAULT_NUM_ELLIPSES;

    if (argc > 5) {
        std::cerr << "Usage: " << argv[0] << " [host] [port] [seed] [num_ellipses]" << std::endl;
        return 1;
    }

    try {
        if (argc >= 2)
            host = argv[1];
        if (argc >= 3) {
            port = std::stoi(argv[2]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Error: Port number must be between 1 and 65535." << std::endl;
                return 1;
            }
        }
        if (argc >= 4) {
            seed = static_cast<unsigned int>(std::stoul(argv[3]));
        }
        if (argc >= 5) {
            num_ellipses = std::stoi(argv[4]);
            if (num_ellipses <= 0) {
                std::cerr << "Error: Number of ellipses must be positive." << std::endl;
                return 1;
            }
        }
    } catch (const std::invalid_argument &e) {
        std::cerr << "Error: Invalid argument type provided. " << e.what() << std::endl;
        return 1;
    } catch (const std::out_of_range &e) {
        std::cerr << "Error: Argument value out of range. " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Client configured with:" << std::endl;
    std::cout << "  Host: " << host << std::endl;
    std::cout << "  Port: " << port << std::endl;
    std::cout << "  Seed: " << seed << std::endl;
    std::cout << "  Number of Ellipses: " << num_ellipses << std::endl;

    Client::TcpClient client(host, port);
    if (!client.connect_to_server()) {
        std::cerr << "Failed to connect to server." << std::endl;
        return 1;
    }

    Client::EllipseGenerator generator(seed);
    for (int i = 0; i < num_ellipses; ++i) {
        Ellipse e = generator.generate_ellipse();
        std::cout << "\n--- Sending Ellipse " << i + 1 << "/" << num_ellipses << " ---" << std::endl;
        if (!client.send_ellipse_and_get_response(e)) {
            std::cerr << "Error during communication for ellipse " << i + 1 << "." << std::endl;
            break;
        }
    }

    client.disconnect();
    return 0;
}