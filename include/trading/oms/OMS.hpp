#pragma once

#include "trading/core/Types.hpp"

namespace trading {

class OMS {
public:
    OMS() = default;

    [[nodiscard]] OrderId next_order_id() noexcept;

private:
    OrderId next_order_id_{1};
};

} // namespace trading

