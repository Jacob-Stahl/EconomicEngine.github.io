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

        void matchFound(long makeId, long takeId, unsigned int transferQty);
        void cancelled(long ordId, unsigned int remainingQty);
        bool getOrder(long ordId, Order& order) const;

        std::unordered_map<long, Order> orderRegistery;
        std::vector<Match2> matches;
        std::vector<Order> cancellations;
};