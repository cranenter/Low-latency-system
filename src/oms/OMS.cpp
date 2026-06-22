#include "trading/oms/OMS.hpp"

namespace trading {

OrderId OMS::next_order_id() noexcept
{
    return next_order_id_++;
}

} // namespace trading

