#pragma once

#include "../include/core/limit_order_book.hpp"

namespace raijin::test
{
    inline BookConfig tiny_config()
    {
        return BookConfig{
            .order_pool_capacity = 8,
            .price_level_count = 256,
            .level_queue_capacity = 4,
            .max_order_id = 64};
    }

    inline BookConfig small_config()
    {
        return BookConfig{
            .order_pool_capacity = 256,
            .price_level_count = 8192,
            .level_queue_capacity = 64,
            .max_order_id = 4096};
    }
}
