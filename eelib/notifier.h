#pragma once

#include "match.h"
#include "order.h"
#include <unordered_map>

// TODO notify filled orders
// TODO remove filled orders from registery

class Notifier{
    public:
        void matchFound(Order& make, Order& take, std::uint32_t transferQty);
        void cancelled(std::int64_t order);

        std::vector<Match> matches;
        std::vector<std::int64_t> cancellations;
};