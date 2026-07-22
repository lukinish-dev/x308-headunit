#pragma once

#include <iosfwd>
#include <memory>

namespace x308 {

struct AppContext;

enum class ApplicationState { created, initialized, running, stopping, stopped };

class Application {
public:
    Application();
    Application(std::istream& input, std::ostream& output, std::ostream& error);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] int run(int argc, const char* const* argv);
    [[nodiscard]] ApplicationState state() const noexcept;

private:
    void shutdown() noexcept;
    [[nodiscard]] int finish(int exitCode) noexcept;

    std::istream& input_;
    std::ostream& output_;
    std::ostream& error_;
    ApplicationState state_{ApplicationState::created};
    std::unique_ptr<AppContext> context_;
};

using App = Application;

}  // namespace x308
