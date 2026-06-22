#include "trading/position/position_manager.hpp"

#include <cstdlib>

namespace trading {

void PositionManager::apply_fill(const ExecutionReport& report) noexcept
{
    apply(report.symbol, report.side, report.last_price, report.last_quantity);
}

void PositionManager::apply_trade(const Trade& trade) noexcept
{
    apply(trade.symbol, trade.side, trade.price, trade.quantity);
}

std::int64_t PositionManager::current_position(const Symbol& symbol) const noexcept
{
    const auto it = positions_.find(key(symbol));
    return it == positions_.end() ? 0 : it->second.quantity;
}

Price PositionManager::average_price(const Symbol& symbol) const noexcept
{
    const auto it = positions_.find(key(symbol));
    return it == positions_.end() ? 0 : it->second.average_price;
}

std::int64_t PositionManager::realized_pnl(const Symbol& symbol) const noexcept
{
    const auto it = positions_.find(key(symbol));
    return it == positions_.end() ? 0 : it->second.realized_pnl;
}

std::int64_t PositionManager::position() const noexcept
{
    std::int64_t total = 0;
    for (const auto& entry : positions_) {
        total += entry.second.quantity;
    }
    return total;
}

Quantity PositionManager::total_bought() const noexcept
{
    Quantity total = 0;
    for (const auto& entry : positions_) {
        total += entry.second.total_bought;
    }
    return total;
}

Quantity PositionManager::total_sold() const noexcept
{
    Quantity total = 0;
    for (const auto& entry : positions_) {
        total += entry.second.total_sold;
    }
    return total;
}

std::string PositionManager::summary() const
{
    std::ostringstream out;
    out << "positions=" << positions_.size();
    for (const auto& entry : positions_) {
        out << " " << entry.first
            << ":qty=" << entry.second.quantity
            << ",avg=" << entry.second.average_price
            << ",realized=" << entry.second.realized_pnl;
    }
    return out.str();
}

void PositionManager::apply(Symbol symbol, Side side, Price price, Quantity quantity) noexcept
{
    if (symbol.empty() || price <= 0 || quantity <= 0) {
        return;
    }

    PositionState& state = positions_[key(symbol)];
    const std::int64_t signed_fill = side == Side::Buy ? quantity : -static_cast<std::int64_t>(quantity);

    if (side == Side::Buy) {
        state.total_bought += quantity;
    } else {
        state.total_sold += quantity;
    }

    if (state.quantity == 0 || (state.quantity > 0 && signed_fill > 0)
        || (state.quantity < 0 && signed_fill < 0)) {
        const std::int64_t current_abs = std::llabs(state.quantity);
        const std::int64_t fill_abs = quantity;
        const std::int64_t new_abs = current_abs + fill_abs;
        state.average_price = static_cast<Price>(
            (static_cast<std::int64_t>(state.average_price) * current_abs
                + static_cast<std::int64_t>(price) * fill_abs)
            / new_abs);
        state.quantity += signed_fill;
        return;
    }

    const std::int64_t closing_quantity = std::min<std::int64_t>(std::llabs(state.quantity), quantity);
    if (state.quantity > 0 && side == Side::Sell) {
        state.realized_pnl += (price - state.average_price) * closing_quantity;
    } else if (state.quantity < 0 && side == Side::Buy) {
        state.realized_pnl += (state.average_price - price) * closing_quantity;
    }

    state.quantity += signed_fill;

    if (state.quantity == 0) {
        state.average_price = 0;
    } else if ((state.quantity > 0 && signed_fill > 0) || (state.quantity < 0 && signed_fill < 0)) {
        state.average_price = price;
    } else if (std::llabs(signed_fill) > closing_quantity) {
        state.average_price = price;
    }
}

std::string PositionManager::key(const Symbol& symbol)
{
    return std::string(symbol.view());
}

} // namespace trading

