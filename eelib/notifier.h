#pragma once

#include "match.h"
#include "order.h"
#include <unordered_map>

// TODO notify filled orders
// TODO remove filled orders from registery

class Notifier{
    public:
        void registerOrder(const Order& order){
            orderRegistery.insert({order.ordId, order});
        }

        void matchFound(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty);
        void cancelled(std::int64_t ordId, std::uint32_t remainingQty);
        bool getOrder(std::int64_t ordId, Order& order) const;

        std::unordered_map<std::int64_t, Order> orderRegistery;
        std::vector<Match> matches;
        std::vector<Order> cancellations;
};