#pragma once

#include "trading/core/Events.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace trading {

class MarketDataReplay {
public:
    MarketDataReplay() = default;

    explicit MarketDataReplay(const std::string& csv_path)
    {
        const bool loaded = load_csv(csv_path);
        (void)loaded;
    }

    // Real direct feeds are commonly sequenced event streams. Production feed
    // handlers decode venue-specific binary protocols and use sequence numbers
    // for gap detection and recovery. This simulator models that concept with
    // a simple CSV format and in-memory event vector.
    [[nodiscard]] bool load_csv(const std::string& csv_path);

    void generate_synthetic(std::size_t event_count);
    void clear() noexcept;

    [[nodiscard]] const std::vector<MarketDataEvent>& events() const noexcept;
    [[nodiscard]] std::size_t loaded_event_count() const noexcept;
    [[nodiscard]] bool sequence_order_valid() const noexcept;
    [[nodiscard]] std::size_t invalid_line_count() const noexcept;

private:
    std::vector<MarketDataEvent> events_{};
    std::size_t invalid_line_count_{0};
    bool sequence_order_valid_{true};
};

} // namespace trading
