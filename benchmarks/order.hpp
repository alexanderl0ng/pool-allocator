#pragma once

#include "types.hpp"

namespace lob {

struct Limit;

enum class Side : u8 { Buy, Sell };

enum class OrderType : u8 { Limit, Market };

enum class TimeInForce : u8 {
    GoodTillCancel,     // stays in the book until filled or cancelled
    ImmediateOrCancel,  // fill what you can immediately, cancel the rest
    FillOrKill,         // fill completely now, or cancel entirely
    Day,                // rests in the book till filled or cancelled at end of day
};

struct alignas(64) Order {
    u64         id_number;      // 8  
    u64         limit;          // 8
    u64         entry_time;     // 8
    u64         event_time;     // 8
    Order*      next_order;     // 8
    Order*      prev_order;     // 8
    Limit*      parent_limit;   // 8
    u32         shares;         // 4
    Side        side;           // 1
    OrderType   type;           // 1
    TimeInForce time_in_force;  // 1
};
static_assert(sizeof(Order) == 64);

} // namespace lob
    
