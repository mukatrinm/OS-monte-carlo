#include "reactor.h"
#include <cstring>
#include <iostream>
#include <sys/time.h>

Reactor::Reactor() : fdmax_{-1} {
    FD_ZERO(&master_);
    FD_ZERO(&read_fds_);
}

Reactor::~Reactor() {
    stop();
    wait();

    // Clean up all file descriptors if still open
    for (int fd = 0; fd <= fdmax_; fd++) {
        if (FD_ISSET(fd, &master_)) {
            // std::cout << "Reactor DTOR: Closing fd " << fd << std::endl;
            FD_CLR(fd, &master_);
            close(fd);
            handlers_[fd] = nullptr;
        }
    }

    fdmax_ = -1;
}

void Reactor::start() {
    std::cout << "Reactor::start: Starting reactor thread." << std::endl;
    if (running_.exchange(true))
        return;
    thread_ = std::thread(&Reactor::run, this);
}

void Reactor::stop() {
    std::cout << "Reactor::stop: Stopping reactor thread." << std::endl;
    running_.store(false);
}

void Reactor::wait() {
    if (thread_.joinable())
        thread_.join();
}

void Reactor::addFd(int newfd, HandlerCallback handler) {
    if (newfd < 0 || newfd >= FD_SETSIZE) {
        std::cerr << "Reactor::addFd: Invalid fd " << newfd << std::endl;
        return;
    }
    FD_SET(newfd, &master_);
    handlers_[newfd] = std::move(handler);

    if (newfd > fdmax_)
        fdmax_ = newfd;
}

void Reactor::removeFd(int fd) {
    if (fd < 0 || fd >= FD_SETSIZE)
        return;

    FD_CLR(fd, &master_);
    handlers_[fd] = nullptr;

    /* shrink fdmax_ if we removed the highest fd */
    if (fd == fdmax_) {
        while (fdmax_ >= 0 && !FD_ISSET(fdmax_, &master_))
            fdmax_--;
    }
}

void Reactor::run() {
    std::cout << "Reactor::run: Event loop started." << std::endl;
    struct timeval select_timeout;

    while (running_.load()) {
        read_fds_ = master_;

        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 500000;

        int n_ready = select(fdmax_ + 1, &read_fds_, nullptr, nullptr, &select_timeout);

        if (!running_.load()) {
            std::cout << "Reactor::run: Stopping reactor thread." << std::endl;
            break;
        }

        if (n_ready == -1) {
            perror("Reactor::run select");
            running_.store(false);

            break;
        }

        for (int fd = 0; fd <= fdmax_; fd++) {
            if (FD_ISSET(fd, &read_fds_)) {
                if (handlers_[fd]) {
                    try {
                        handlers_[fd](fd);
                    } catch (const std::exception &e) {
                        std::cerr << "Reactor: Exception in handler for fd " << fd << ": " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Reactor: Unknown exception in handler for fd " << fd << std::endl;
                    }
                }
            }
        }
    }
}