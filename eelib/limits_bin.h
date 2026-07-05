#pragma once

#include "order.h"
#include "notifier.h"
#include <queue>
#include <unordered_set>

struct StopEntry{
    Order entry;
    TimeInForce timeInForce;
    OrdType type;
    std::int32_t limitPrice;
    bool isCancelled = false;

    StopEntry(const Order& order) :
        timeInForce(order.timeInForce),
        type(order.type),
        limitPrice(order.price),
        entry(order){}
};

class LimitsBin{
    private:
        std::vector<StopEntry> dormantStops;
        std::unordered_set<std::uint64_t> cancelledIds; // TODO: consider using pool allocator here
        std::deque<Order> entries;
        //void notifyMatch(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty);

    public:

        Notifier* notifier = nullptr;

        /// @brief Takes raw pointer to notifier in Matcher. LimitsBin has no effect on the nofifier lifetime
        /// @param _notifier 

        LimitsBin() : notifier(nullptr) {};
        LimitsBin(Notifier* _notifier): notifier(_notifier){};
        std::uint32_t totalQty();
        void make(Order& makeEntry);
        void take(Order& takeEntry, std::unordered_map<std::int64_t, Order>& orderRegistry);
        bool cancelLimit(std::int64_t ordId);
        bool cancelStop(std::int64_t ordId); // TODO: add test coverage for stop cancellation

        bool hasDormantStops() const;
        void addDormantStop(const StopEntry& dormantStop);
        void moveAllStopsToActive(std::vector<StopEntry>& activeStops);
        bool isEmpty() const;
};
