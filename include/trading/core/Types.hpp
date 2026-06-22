#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace trading {

using Price = std::int64_t;
using Quantity = std::int32_t;
using OrderId = std::uint64_t;
using ClientOrderId = std::uint64_t;
using Timestamp = std::uint64_t;

class Symbol {
public:
    static constexpr std::size_t MaxLength = 16;

    Symbol() noexcept = default;

    explicit Symbol(std::string_view value) noexcept
    {
        assign(value);
    }

    void assign(std::string_view value) noexcept
    {
        value_.fill('\0');
        const std::size_t count = value.size() < MaxLength - 1 ? value.size() : MaxLength - 1;
        for (std::size_t i = 0; i < count; ++i) {
            value_[i] = value[i];
        }
    }

    [[nodiscard]] const char* c_str() const noexcept
    {
        return value_.data();
    }

    [[nodiscard]] std::string_view view() const noexcept
    {
        std::size_t len = 0;
        while (len < MaxLength && value_[len] != '\0') {
            ++len;
        }
        return std::string_view(value_.data(), len);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return value_[0] == '\0';
    }

    friend bool operator==(const Symbol& lhs, const Symbol& rhs) noexcept
    {
        return lhs.value_ == rhs.value_;
    }

    friend bool operator!=(const Symbol& lhs, const Symbol& rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    // Fixed-size storage avoids heap allocation in hot-path event objects.
    // Real systems often use numeric symbol IDs after startup-time lookup.
    std::array<char, MaxLength> value_{};
};

enum class Side {
    Buy,
    Sell
};

enum class OrderType {
    Limit,
    Market
};

enum class TimeInForce {
    Day,
    IOC,
    FOK
};

enum class OrderStatus {
    Created,
    New = Created,
    PendingNew,
    Acked,
    Live = Acked,
    PartiallyFilled,
    Filled,
    PendingCancel,
    Canceled,
    Cancelled = Canceled,
    Rejected
};

enum class RejectReason {
    None,
    InvalidPrice,
    InvalidQuantity,
    RiskLimitExceeded,
    UnknownOrder,
    ExchangeReject
};

enum class MdEventType {
    Add,
    Modify,
    Cancel,
    Trade
};

enum class ExecType {
    Ack,
    NewAck = Ack,
    Reject,
    NewReject = Reject,
    PartialFill,
    Fill,
    CancelAck,
    CancelReject
};

} // namespace trading
