#pragma once

#include "trading/core/Events.hpp"
#include "trading/md/order_book.hpp"

#include <array>
#include <cstddef>

namespace trading {

struct StrategyConfig {
    Symbol symbol{};
    Quantity order_quantity{1};
    Price max_spread{0};
    bool generate_buy{true};
    bool generate_sell{false};
};

class GeneratedOrders {
public:
    static constexpr std::size_t MaxOrders = 2;

    [[nodiscard]] bool push(const Order& order) noexcept
    {
        if (count_ == MaxOrders) {
            return false;
        }
        orders_[count_++] = order;
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return count_;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return count_ == 0;
    }

    [[nodiscard]] const Order& operator[](std::size_t index) const noexcept
    {
        return orders_[index];
    }

private:
    std::array<Order, MaxOrders> orders_{};
    std::size_t count_{0};
};

class StrategySimulator {
public:
    explicit StrategySimulator(StrategyConfig config) noexcept;

    // This is not an alpha model. It deliberately uses simple best bid/ask
    // logic to drive the trading infrastructure pipeline. Real strategies
    // include richer signals, inventory controls, venue models, and risk-aware
    // order placement.
    [[nodiscard]] GeneratedOrders on_book_update(const OrderBook& book, Timestamp timestamp) noexcept;

    [[nodiscard]] const StrategyConfig& config() const noexcept;

private:
    StrategyConfig config_{};
    ClientOrderId next_client_order_id_{1};
};

} // namespace trading

