#pragma once

#include "trading/core/Events.hpp"

#include <array>
#include <cstddef>

namespace trading {

enum class RiskRejectReason {
    None,
    DisallowedSymbol,
    InvalidQuantity,
    InvalidPrice,
    QuantityLimitExceeded,
    NotionalLimitExceeded,
    PositionLimitExceeded,
    PriceBandExceeded,
    OrderRateExceeded
};

struct RiskResult {
    bool accepted{false};
    RiskRejectReason reason{RiskRejectReason::None};

    [[nodiscard]] static RiskResult accept() noexcept
    {
        return RiskResult{true, RiskRejectReason::None};
    }

    [[nodiscard]] static RiskResult reject(RiskRejectReason reject_reason) noexcept
    {
        return RiskResult{false, reject_reason};
    }
};

struct RiskMarketState {
    Price best_bid{};
    Price best_ask{};
    Price reference_price{};
};

struct RiskConfig {
    static constexpr std::size_t MaxAllowedSymbols = 16;

    std::array<Symbol, MaxAllowedSymbols> allowed_symbols{};
    std::size_t allowed_symbol_count{0};
    Quantity max_order_quantity{0};
    std::int64_t max_notional{0};
    std::int64_t max_abs_position{0};
    Price price_band_ticks{0};
    std::size_t max_orders_per_window{0};
    Timestamp order_rate_window_ns{0};

    bool add_allowed_symbol(Symbol symbol) noexcept;
};

class PreTradeRiskEngine {
public:
    explicit PreTradeRiskEngine(RiskConfig config) noexcept;

    // Pre-trade risk is on the hot path: every outbound order must pass these
    // checks before reaching a gateway. This demo implements lightweight local
    // checks. Production systems usually add account limits, fat-finger bands,
    // kill switches, credit controls, exchange throttles, drop-copy
    // reconciliation, and independent risk-service enforcement.
    [[nodiscard]] RiskResult validate(
        const Order& order,
        std::int64_t current_position,
        const RiskMarketState& market,
        Timestamp now) noexcept;

    [[nodiscard]] const RiskConfig& config() const noexcept;

private:
    [[nodiscard]] bool symbol_allowed(const Symbol& symbol) const noexcept;
    [[nodiscard]] bool price_inside_band(const Order& order, const RiskMarketState& market) const noexcept;
    [[nodiscard]] bool order_rate_available(Timestamp now) noexcept;
    void record_order_attempt(Timestamp now) noexcept;

    RiskConfig config_{};
    Timestamp window_start_ns_{0};
    std::size_t orders_in_window_{0};
};

} // namespace trading

