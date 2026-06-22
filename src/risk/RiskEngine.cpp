#include "trading/risk/RiskEngine.hpp"

namespace trading {

bool RiskEngine::allow(const OrderRequest& request) const noexcept
{
    return request.price > 0 && request.quantity > 0;
}

bool RiskEngine::allow(const Order& order) const noexcept
{
    return order.price > 0 && order.quantity > 0;
}

} // namespace trading
