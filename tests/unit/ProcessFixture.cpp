#include <csignal>
#include <iostream>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>

int main(const int argc, const char* const* argv) {
    if (argc != 2) return 2;
    const std::string_view mode{argv[1]};
    if (mode == "output") {
        std::cout << "fixture stdout\n";
        std::cerr << "fixture stderr\n";
        return 7;
    }
    if (mode == "hang-tree") {
        static_cast<void>(signal(SIGTERM, SIG_IGN));
        const pid_t descendant = fork();
        if (descendant < 0) return 3;
        static_cast<void>(signal(SIGTERM, SIG_IGN));
        std::cout << "fixture ready\n" << std::flush;
        while (true) pause();
    }
    return 2;
}
