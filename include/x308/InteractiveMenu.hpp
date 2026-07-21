#pragma once

#include <iosfwd>

namespace x308 { class IMediaPlayer; }

namespace x308 {

class InteractiveMenu {
public:
    explicit InteractiveMenu(IMediaPlayer* mediaPlayer = nullptr);
    [[nodiscard]] int run(std::istream& input, std::ostream& output) const;

private:
    [[nodiscard]] int runMpdMenu(std::istream& input, std::ostream& output) const;
    IMediaPlayer* mediaPlayer_;
};

}  // namespace x308
