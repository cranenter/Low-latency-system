#include "trading/risk/pre_trade_risk_engine.hpp"

#include <cstdint>

namespace trading {

bool RiskConfig::add_allowed_symbol(Symbol symbol) noexcept
{
    if (allowed_symbol_count == MaxAllowedSymbols) {
        return false;
    }

    allowed_symbols[allowed_symbol_count++] = symbol;
    return true;
}

PreTradeRiskEngine::PreTradeRiskEngine(RiskConfig config) noexcept
    : config_(config)
{
}

RiskResult PreTradeRiskEngine::validate(
    const Order& order,
    std::int64_t current_position,
    const RiskMarketState& market,
    Timestamp now) noexcept
{
    if (!symbol_allowed(order.symbol)) {
        return RiskResult::reject(RiskRejectReason::DisallowedSymbol);
    }

    if (order.quantity <= 0) {
        return RiskResult::reject(RiskRejectReason::InvalidQuantity);
    }

    if (order.price <= 0) {
        return RiskResult::reject(RiskRejectReason::InvalidPrice);
    }

    if (config_.max_order_quantity > 0 && order.quantity > config_.max_order_quantity) {
        return RiskResult::reject(RiskRejectReason::QuantityLimitExceeded);
    }

    const auto notional =
        static_cast<std::int64_t>(order.price) * static_cast<std::int64_t>(order.quantity);
    if (config_.max_notional > 0 && notional > config_.max_notional) {
        return RiskResult::reject(RiskRejectReason::NotionalLimitExceeded);
    }

    const std::int64_t signed_order_quantity =
        order.side == Side::Buy ? order.quantity : -static_cast<std::int64_t>(order.quantity);
    const std::int64_t projected_position = current_position + signed_order_quantity;
    const std::int64_t abs_projected =
        projected_position < 0 ? -projected_position : projected_position;
    if (config_.max_abs_position > 0 && abs_projected > config_.max_abs_position) {
        return RiskResult::reject(RiskRejectReason::PositionLimitExceeded);
    }

    if (!price_inside_band(order, market)) {
        return RiskResult::reject(RiskRejectReason::PriceBandExceeded);
    }

    if (!order_rate_available(now)) {
        return RiskResult::reject(RiskRejectReason::OrderRateExceeded);
    }

    record_order_attempt(now);
    return RiskResult::accept();
}

const RiskConfig& PreTradeRiskEngine::config() const noexcept
{
    return config_;
}

bool PreTradeRiskEngine::symbol_allowed(const Symbol& symbol) const noexcept
{
    if (config_.allowed_symbol_count == 0) {
        return true;
    }

    for (std::size_t i = 0; i < config_.allowed_symbol_count; ++i) {
        if (config_.allowed_symbols[i] == symbol) {
            return true;
        }
    }
    return false;
}

bool PreTradeRiskEngine::price_inside_band(const Order& order, const RiskMarketState& market) const noexcept
{
    if (config_.price_band_ticks <= 0) {
        return true;
    }

    Price lower_bound = 0;
    Price upper_bound = 0;

    if (market.best_bid > 0 && market.best_ask > 0) {
        lower_bound = market.best_bid - config_.price_band_ticks;
        upper_bound = market.best_ask + config_.price_band_ticks;
    } else if (market.reference_price > 0) {
        lower_bound = market.reference_price - config_.price_band_ticks;
        upper_bound = market.reference_price + config_.price_band_ticks;
    } else {
        return true;
    }

    return order.price >= lower_bound && order.price <= upper_bound;
}

bool PreTradeRiskEngine::order_rate_available(Timestamp now) noexcept
{
    if (config_.max_orders_per_window == 0 || config_.order_rate_window_ns == 0) {
        return true;
    }

    if (window_start_ns_ == 0 || now - window_start_ns_ >= config_.order_rate_window_ns) {
        window_start_ns_ = now;
        orders_in_window_ = 0;
    }

    return orders_in_window_ < config_.max_orders_per_window;
}

void PreTradeRiskEngine::record_order_attempt(Timestamp now) noexcept
{
    if (config_.max_orders_per_window == 0 || config_.order_rate_window_ns == 0) {
        return;
    }

    if (window_start_ns_ == 0 || now - window_start_ns_ >= config_.order_rate_window_ns) {
        window_start_ns_ = now;
        orders_in_window_ = 0;
    }
    ++orders_in_window_;
}

} // namespace trading

