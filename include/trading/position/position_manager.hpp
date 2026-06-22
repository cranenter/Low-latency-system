#pragma once

#include "trading/core/Events.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>

namespace trading {

class PositionManager {
public:
    PositionManager() = default;

    // Position is needed by pre-trade risk for projected-position checks before
    // orders leave the system. Production systems also reconcile positions
    // against broker, clearing, exchange, and account state; this demo keeps a
    // deterministic in-memory view from fills only.
    void apply_fill(const ExecutionReport& report) noexcept;
    void apply_trade(const Trade& trade) noexcept;

    [[nodiscard]] std::int64_t current_position(const Symbol& symbol) const noexcept;
    [[nodiscard]] Price average_price(const Symbol& symbol) const noexcept;
    [[nodiscard]] std::int64_t realized_pnl(const Symbol& symbol) const noexcept;

    [[nodiscard]] std::int64_t position() const noexcept;
    [[nodiscard]] Quantity total_bought() const noexcept;
    [[nodiscard]] Quantity total_sold() const noexcept;
    [[nodiscard]] std::string summary() const;

private:
    struct PositionState {
        std::int64_t quantity{};
        Price average_price{};
        std::int64_t realized_pnl{};
        Quantity total_bought{};
        Quantity total_sold{};
    };

    void apply(Symbol symbol, Side side, Price price, Quantity quantity) noexcept;
    [[nodiscard]] static std::string key(const Symbol& symbol);

    std::unordered_map<std::string, PositionState> positions_;
};

} // namespace trading

