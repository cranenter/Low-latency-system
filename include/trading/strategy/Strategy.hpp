#pragma once

namespace trading {

class Strategy {
public:
    Strategy() = default;

    [[nodiscard]] const char* name() const noexcept;
};

} // namespace trading

