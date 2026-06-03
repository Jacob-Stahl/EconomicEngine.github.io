#pragma once

#include "order.h"
#include "notifier.h"
#include <queue>

struct BookEntry{
    std::int64_t ordId = -1;
    std::uint32_t qty = 0;
    //bool isCancelled = false;
    BookEntry(const Order& order) : 
        ordId(order.ordId), 
        qty(order.qty){}
};

struct StopEntry{
    BookEntry entry;
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

// TODO: Store all stops at this price, on this side. 
//      matcher will place all stops when stop price is hit

class LimitsBin{
    private:
        //std::vector<std::int64_t> cancelledLimits;
        std::vector<StopEntry> dormantStops;
        std::vector<std::uint64_t> cancelledLimits;
        size_t numCancelledLimits = 0;
        std::deque<BookEntry> entries;
        std::uint32_t _totalQty = 0;
        void notifyMatch(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty);
        bool findEraseCancelledLimit(std::int64_t orderId);

    public:

        Notifier* notifier = nullptr;

        /// @brief Takes raw pointer to notifier in Matcher. LimitsBin has no effect on the nofifier lifetime
        /// @param _notifier 

        LimitsBin() : notifier(nullptr) {};
        LimitsBin(Notifier* _notifier): notifier(_notifier){};
        const std::uint32_t totalQty() const {return _totalQty; };
        void make(const BookEntry& makeEntry);
        void take(BookEntry& takeEntry);
        void cancelLimit(std::int64_t ordId);
        void cancelStop(std::int64_t ordId); // TODO: add test coverage for stop cancellation

        bool hasDormantStops() const;
        void addDormantStop(const StopEntry& dormantStop);
        void moveAllStopsToActive(std::vector<StopEntry>& activeStops);
};
