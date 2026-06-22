#include "trading/md/order_book.hpp"

#include <algorithm>

namespace trading {

bool OrderBook::apply(const MarketDataEvent& event)
{
    switch (event.type) {
    case MdEventType::Add:
        return add_order(event);
    case MdEventType::Modify:
        return modify_order(event);
    case MdEventType::Cancel:
        return cancel_order(event);
    case MdEventType::Trade:
        return trade_order(event);
    }
    return false;
}

void OrderBook::clear() noexcept
{
    bids_.clear();
    asks_.clear();
    orders_.clear();
}

bool OrderBook::has_bid() const noexcept
{
    return !bids_.empty();
}

bool OrderBook::has_ask() const noexcept
{
    return !asks_.empty();
}

Price OrderBook::best_bid() const noexcept
{
    return bids_.empty() ? 0 : bids_.begin()->first;
}

Price OrderBook::best_ask() const noexcept
{
    return asks_.empty() ? 0 : asks_.begin()->first;
}

Price OrderBook::spread() const noexcept
{
    if (!has_bid() || !has_ask()) {
        return 0;
    }
    return best_ask() - best_bid();
}

Quantity OrderBook::quantity_at(Side side, Price price) const
{
    if (side == Side::Buy) {
        const auto it = bids_.find(price);
        return it == bids_.end() ? 0 : it->second;
    }

    const auto it = asks_.find(price);
    return it == asks_.end() ? 0 : it->second;
}

bool OrderBook::has_order(OrderId market_data_order_id) const
{
    return orders_.find(market_data_order_id) != orders_.end();
}

std::vector<PriceLevel> OrderBook::top_levels(Side side, std::size_t depth) const
{
    std::vector<PriceLevel> levels;
    levels.reserve(depth);

    if (side == Side::Buy) {
        for (const auto& [price, quantity] : bids_) {
            if (levels.size() == depth) {
                break;
            }
            levels.push_back(PriceLevel{price, quantity});
        }
        return levels;
    }

    for (const auto& [price, quantity] : asks_) {
        if (levels.size() == depth) {
            break;
        }
        levels.push_back(PriceLevel{price, quantity});
    }
    return levels;
}

bool OrderBook::add_order(const MarketDataEvent& event)
{
    if (event.market_data_order_id == 0 || event.price <= 0 || event.quantity <= 0) {
        return false;
    }

    if (orders_.find(event.market_data_order_id) != orders_.end()) {
        return false;
    }

    orders_.emplace(event.market_data_order_id,
        BookOrder{event.market_data_order_id, event.side, event.price, event.quantity});
    add_quantity(event.side, event.price, event.quantity);
    return true;
}

bool OrderBook::modify_order(const MarketDataEvent& event)
{
    if (event.price <= 0 || event.quantity <= 0) {
        return false;
    }

    const auto it = orders_.find(event.market_data_order_id);
    if (it == orders_.end()) {
        return false;
    }

    BookOrder& order = it->second;
    remove_quantity(order.side, order.price, order.quantity);

    order.side = event.side;
    order.price = event.price;
    order.quantity = event.quantity;

    add_quantity(order.side, order.price, order.quantity);
    return true;
}

bool OrderBook::cancel_order(const MarketDataEvent& event)
{
    const auto it = orders_.find(event.market_data_order_id);
    if (it == orders_.end()) {
        return false;
    }

    const BookOrder order = it->second;
    remove_quantity(order.side, order.price, order.quantity);
    orders_.erase(it);
    return true;
}

bool OrderBook::trade_order(const MarketDataEvent& event)
{
    if (event.quantity <= 0) {
        return false;
    }

    const auto it = orders_.find(event.market_data_order_id);
    if (it == orders_.end()) {
        return false;
    }

    BookOrder& order = it->second;
    const Quantity traded = std::min(order.quantity, event.quantity);
    remove_quantity(order.side, order.price, traded);
    order.quantity -= traded;

    if (order.quantity == 0) {
        orders_.erase(it);
    }
    return true;
}

void OrderBook::add_quantity(Side side, Price price, Quantity quantity)
{
    if (side == Side::Buy) {
        bids_[price] += quantity;
        return;
    }
    asks_[price] += quantity;
}

void OrderBook::remove_quantity(Side side, Price price, Quantity quantity)
{
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) {
            return;
        }
        it->second -= quantity;
        if (it->second <= 0) {
            bids_.erase(it);
        }
        return;
    }

    auto it = asks_.find(price);
    if (it == asks_.end()) {
        return;
    }
    it->second -= quantity;
    if (it->second <= 0) {
        asks_.erase(it);
    }
}

} // namespace trading

