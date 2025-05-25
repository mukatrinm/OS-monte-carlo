#pragma once
#include <atomic>
#include <functional>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

class Reactor {
public:
    using HandlerCallback = std::function<void(int fd)>;

    Reactor();
    ~Reactor();

    void start();
    void stop();
    void wait();

    void addFd(int newfd, HandlerCallback handler);
    void removeFd(int fd);

private:
    void run();

    fd_set master_;
    fd_set read_fds_;
    int fdmax_;

    HandlerCallback handlers_[FD_SETSIZE]{};

    std::atomic<bool> running_{false};
    std::thread thread_;
};