#pragma once

#include "trading/core/Events.hpp"

namespace trading {

class RiskEngine {
public:
    RiskEngine() = default;

    [[nodiscard]] bool allow(const OrderRequest& request) const noexcept;
    [[nodiscard]] bool allow(const Order& order) const noexcept;
};

} // namespace trading
