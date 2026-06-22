#include "trading/strategy/strategy_simulator.hpp"

namespace trading {

StrategySimulator::StrategySimulator(StrategyConfig config) noexcept
    : config_(config)
{
}

GeneratedOrders StrategySimulator::on_book_update(const OrderBook& book, Timestamp timestamp) noexcept
{
    GeneratedOrders generated;

    if (!book.has_bid() || !book.has_ask()) {
        return generated;
    }

    if (book.spread() > config_.max_spread || config_.order_quantity <= 0) {
        return generated;
    }

    if (config_.generate_buy) {
        const bool pushed = generated.push(Order(
            next_client_order_id_++,
            config_.symbol,
            Side::Buy,
            OrderType::Limit,
            TimeInForce::Day,
            book.best_bid(),
            config_.order_quantity,
            timestamp));
        (void)pushed;
    }

    if (config_.generate_sell) {
        const bool pushed = generated.push(Order(
            next_client_order_id_++,
            config_.symbol,
            Side::Sell,
            OrderType::Limit,
            TimeInForce::Day,
            book.best_ask(),
            config_.order_quantity,
            timestamp));
        (void)pushed;
    }

    return generated;
}

const StrategyConfig& StrategySimulator::config() const noexcept
{
    return config_;
}

} // namespace trading
