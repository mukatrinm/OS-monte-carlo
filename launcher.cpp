#include <csignal>
#include <cstdlib> // For exit
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

// --- Launcher Defaults ---
const int DEFAULT_NUM_CLIENTS_TO_LAUNCH = 10;
const std::string DEFAULT_CLIENT_EXECUTABLE_PATH = "./client_app";

const std::string DEFAULT_TARGET_SERVER_HOST = "127.0.0.1";
const int DEFAULT_TARGET_SERVER_PORT = 12345;
const unsigned int DEFAULT_BASE_SEED_FOR_CLIENTS = 42;
const int DEFAULT_ELLIPSES_PER_CLIENT_INSTANCE = 10;

void print_usage(const char *prog_name) {
    std::cerr << "Usage: " << prog_name << " [num_clients] [ellipses_per_client] [base_seed] [server_host] [server_port]" << std::endl;
    std::cerr << "  num_clients (int): Number of client instances to launch (default: " << DEFAULT_NUM_CLIENTS_TO_LAUNCH << ")" << std::endl;
    std::cerr << "  ellipses_per_client (int): Number of ellipses each client will send (default: " << DEFAULT_ELLIPSES_PER_CLIENT_INSTANCE << ")" << std::endl;
    std::cerr << "  base_seed (uint): Base seed for generating unique seeds for clients (default: " << DEFAULT_BASE_SEED_FOR_CLIENTS << ")" << std::endl;
    std::cerr << "  server_host (string): Hostname/IP of the server (default: " << DEFAULT_TARGET_SERVER_HOST << ")" << std::endl;
    std::cerr << "  server_port (int): Port number of the server (default: " << DEFAULT_TARGET_SERVER_PORT << ")" << std::endl;
}

int main(int argc, char *argv[]) {
    int num_clients_to_launch = DEFAULT_NUM_CLIENTS_TO_LAUNCH;
    std::string server_host = DEFAULT_TARGET_SERVER_HOST;
    int server_port = DEFAULT_TARGET_SERVER_PORT;
    unsigned int base_seed = DEFAULT_BASE_SEED_FOR_CLIENTS;
    int ellipses_per_client = DEFAULT_ELLIPSES_PER_CLIENT_INSTANCE;

    if (argc > 6) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        if (argc >= 2) { // num_clients
            num_clients_to_launch = std::stoi(argv[1]);
            if (num_clients_to_launch <= 0) {
                std::cerr << "Error: Number of clients must be positive." << std::endl;
                return 1;
            }
        }
        if (argc >= 3) { // ellipses_per_client
            ellipses_per_client = std::stoi(argv[2]);
            if (ellipses_per_client <= 0) {
                std::cerr << "Error: Number of ellipses per client must be positive." << std::endl;
                return 1;
            }
        }
        if (argc >= 4) { // base_seed
            base_seed = static_cast<unsigned int>(std::stoul(argv[3]));
        }
        if (argc >= 5) { // server_host
            server_host = argv[4];
        }
        if (argc >= 6) { // server_port
            server_port = std::stoi(argv[5]);
            if (server_port <= 0 || server_port > 65535) {
                std::cerr << "Error: Server port must be between 1 and 65535." << std::endl;
                return 1;
            }
        }
    } catch (const std::invalid_argument &e) {
        std::cerr << "Error: Invalid argument type provided. " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    } catch (const std::out_of_range &e) {
        std::cerr << "Error: Argument value out of range. " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "Launcher starting with configuration:" << std::endl;
    std::cout << "  Number of Clients to Launch: " << num_clients_to_launch << std::endl;
    std::cout << "  Client Executable: " << DEFAULT_CLIENT_EXECUTABLE_PATH << std::endl;
    std::cout << "  Target Server Host: " << server_host << std::endl;
    std::cout << "  Target Server Port: " << server_port << std::endl;
    std::cout << "  Base Seed for Clients: " << base_seed << std::endl;
    std::cout << "  Ellipses per Client Instance: " << ellipses_per_client << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    std::vector<pid_t> child_pids;

    for (int i = 0; i < num_clients_to_launch; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Launcher: fork failed");
            std::cerr << "Error: Could not launch all clients. Terminating already started clients." << std::endl;
            for (pid_t child_pid_to_kill : child_pids) {
                kill(child_pid_to_kill, SIGTERM);
            }
            for (pid_t child_pid_to_wait : child_pids) {
                waitpid(child_pid_to_wait, nullptr, 0); // Wait for termination to prevent zombie processes
            }
            return 1;
        } else if (pid == 0) { // Child process
            // std::cout << "Launcher: Child PID " << getpid() << " starting client instance " << i + 1 << std::endl;

            // Prepare arguments for client_app: client_app [host] [port] [seed] [num_ellipses]
            std::string current_port_str = std::to_string(server_port);
            std::string client_seed_str = std::to_string(base_seed);
            std::string client_ellipses_str = std::to_string(ellipses_per_client);

            // Create the argv array for execv (must be null-terminated)
            std::vector<std::string> arg_strings;
            arg_strings.push_back(DEFAULT_CLIENT_EXECUTABLE_PATH);
            arg_strings.push_back(server_host);
            arg_strings.push_back(current_port_str);
            arg_strings.push_back(client_seed_str);
            arg_strings.push_back(client_ellipses_str);

            std::vector<char *> client_argv_cstrs;
            for (const auto &s : arg_strings) {
                client_argv_cstrs.push_back(const_cast<char *>(s.c_str()));
            }
            client_argv_cstrs.push_back(nullptr);

            execv(client_argv_cstrs[0], client_argv_cstrs.data());

            // If execv returns, an error occurred
            perror((std::string("Launcher: execv failed for '") + DEFAULT_CLIENT_EXECUTABLE_PATH + "'").c_str());
            _exit(EXIT_FAILURE);
        } else { // Parent process
            child_pids.push_back(pid);
        }
    }

    std::cout << "Launcher: All " << num_clients_to_launch << " client process(es) have been forked." << std::endl;
    if (num_clients_to_launch > 0) {
        std::cout << "Launcher: Waiting for all client processes to complete..." << std::endl;
    }

    int clients_completed_successfully = 0;
    for (size_t i = 0; i < child_pids.size(); ++i) {
        int status;
        pid_t finished_pid = waitpid(child_pids[i], &status, 0);

        if (finished_pid > 0) {
            if (WIFEXITED(status)) {                   // returns true if the child terminated normally
                int exit_status = WEXITSTATUS(status); // returns the exit status of the child
                // std::cout << "Launcher: Client PID " << finished_pid << " (instance " << i + 1 << ") exited with status " << exit_status << std::endl;
                if (exit_status == 0) {
                    clients_completed_successfully++;
                }
            } else if (WIFSIGNALED(status)) { // returns true if the child was terminated by a signal
                std::cout << "Launcher: Client PID " << finished_pid << " (instance " << i + 1 << ") killed by signal " << WTERMSIG(status) << std::endl;
            }
        } else {
            perror("Launcher: waitpid error");
        }
    }

    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "Launcher: " << clients_completed_successfully << " out of " << num_clients_to_launch
              << " client(s) reported successful completion (exit code 0)." << std::endl;
    std::cout << "Launcher: All child processes waited for." << std::endl;

    return 0;
}