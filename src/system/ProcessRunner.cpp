#include "x308/ProcessRunner.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace x308 {
namespace {

void closeFd(int& descriptor) {
    if (descriptor >= 0) {
        static_cast<void>(close(descriptor));
        descriptor = -1;
    }
}

void readAvailable(int& descriptor, std::string& destination) {
    std::array<char, 4096> buffer{};
    constexpr std::size_t maxReadsPerPoll = 16;
    constexpr std::size_t maxCapturedBytes = 1024 * 1024;
    for (std::size_t readIndex = 0; descriptor >= 0 && readIndex < maxReadsPerPoll; ++readIndex) {
        const ssize_t count = read(descriptor, buffer.data(), buffer.size());
        if (count > 0) {
            const auto available = maxCapturedBytes - std::min(destination.size(), maxCapturedBytes);
            const auto received = static_cast<std::size_t>(count);
            destination.append(buffer.data(), std::min(available, received));
        } else if (count == 0) {
            closeFd(descriptor);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            closeFd(descriptor);
        } else {
            break;
        }
    }
}

void signalProcessGroup(const pid_t child, const int signalNumber) {
    if (kill(-child, signalNumber) != 0) {
        static_cast<void>(kill(child, signalNumber));
    }
}

bool reapWithoutBlocking(const pid_t child, int& waitStatus) {
    while (true) {
        const pid_t waited = waitpid(child, &waitStatus, WNOHANG);
        if (waited == child || (waited < 0 && errno == ECHILD)) return true;
        if (waited < 0 && errno == EINTR) continue;
        return false;
    }
}

void reapAfterKill(const pid_t child, int& waitStatus) {
    while (waitpid(child, &waitStatus, 0) < 0 && errno == EINTR) {
    }
}

}  // namespace

ProcessResult PosixProcessRunner::run(const std::string_view executable,
                                     const std::vector<std::string>& arguments,
                                     const std::chrono::milliseconds timeout) {
    ProcessResult result;
    int outputPipe[2]{-1, -1};
    int errorPipe[2]{-1, -1};
    if (pipe2(outputPipe, O_CLOEXEC) != 0 || pipe2(errorPipe, O_CLOEXEC) != 0) {
        result.standardError = std::strerror(errno);
        closeFd(outputPipe[0]); closeFd(outputPipe[1]);
        closeFd(errorPipe[0]); closeFd(errorPipe[1]);
        return result;
    }

    std::string executableString{executable};
    std::vector<std::string> ownedArguments;
    ownedArguments.reserve(arguments.size() + 1);
    ownedArguments.push_back(executableString);
    ownedArguments.insert(ownedArguments.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    argv.reserve(ownedArguments.size() + 1);
    for (auto& argument : ownedArguments) argv.push_back(argument.data());
    argv.push_back(nullptr);

    const pid_t child = fork();
    if (child < 0) {
        result.standardError = std::strerror(errno);
        closeFd(outputPipe[0]); closeFd(outputPipe[1]);
        closeFd(errorPipe[0]); closeFd(errorPipe[1]);
        return result;
    }
    if (child == 0) {
        static_cast<void>(setpgid(0, 0));
        static_cast<void>(dup2(outputPipe[1], STDOUT_FILENO));
        static_cast<void>(dup2(errorPipe[1], STDERR_FILENO));
        closeFd(outputPipe[0]); closeFd(outputPipe[1]);
        closeFd(errorPipe[0]); closeFd(errorPipe[1]);
        execvp(executableString.c_str(), argv.data());
        _exit(127);
    }

    static_cast<void>(setpgid(child, child));
    closeFd(outputPipe[1]);
    closeFd(errorPipe[1]);
    static_cast<void>(fcntl(outputPipe[0], F_SETFL, O_NONBLOCK));
    static_cast<void>(fcntl(errorPipe[0], F_SETFL, O_NONBLOCK));

    const auto effectiveTimeout = std::max(timeout, std::chrono::milliseconds{1});
    const auto deadline = std::chrono::steady_clock::now() + effectiveTimeout;
    int waitStatus = 0;
    bool childExited = false;
    while (!childExited || outputPipe[0] >= 0 || errorPipe[0] >= 0) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            result.timedOut = true;
            signalProcessGroup(child, SIGTERM);
            const auto graceDeadline = now + std::chrono::milliseconds{100};
            while (!childExited && std::chrono::steady_clock::now() < graceDeadline) {
                childExited = reapWithoutBlocking(child, waitStatus);
                if (!childExited) static_cast<void>(poll(nullptr, 0, 5));
            }
            signalProcessGroup(child, SIGKILL);
            if (!childExited) reapAfterKill(child, waitStatus);
            childExited = true;
            readAvailable(outputPipe[0], result.standardOutput);
            readAvailable(errorPipe[0], result.standardError);
            closeFd(outputPipe[0]);
            closeFd(errorPipe[0]);
            break;
        }

        std::array<pollfd, 2> descriptors{{
            {outputPipe[0], POLLIN | POLLHUP, 0}, {errorPipe[0], POLLIN | POLLHUP, 0}}};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const int pollTimeout = static_cast<int>(std::min(remaining, std::chrono::milliseconds{25}).count());
        static_cast<void>(poll(descriptors.data(), descriptors.size(), pollTimeout));
        readAvailable(outputPipe[0], result.standardOutput);
        readAvailable(errorPipe[0], result.standardError);

        if (!childExited) {
            childExited = reapWithoutBlocking(child, waitStatus);
        }
    }
    closeFd(outputPipe[0]);
    closeFd(errorPipe[0]);
    if (WIFEXITED(waitStatus)) result.exitCode = WEXITSTATUS(waitStatus);
    else if (WIFSIGNALED(waitStatus)) result.exitCode = 128 + WTERMSIG(waitStatus);
    return result;
}

}  // namespace x308
