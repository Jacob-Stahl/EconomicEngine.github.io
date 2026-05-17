#pragma once

#include "order.h"
#include "notifier.h"
#include <queue>

struct BookEntry{
    long ordId = -1;
    unsigned int qty = 0;
    bool isCancelled = false;
    BookEntry(const Order& order) : 
        ordId(order.ordId), 
        qty(order.qty){}
};

struct StopEntry{
    TimeInForce timeInForce;
    OrdType type;
    int limitPrice;
    BookEntry entry;

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
        std::vector<StopEntry> dormantStops;
        std::deque<BookEntry> entries;
        unsigned int _totalQty = 0;
        void notifyMatch(long makeId, long takeId, unsigned int transferQty);

    public:

        Notifier* notifier = nullptr;

        /// @brief Takes raw pointer to notifier in Matcher. LimitsBin has no effect on the nofifier lifetime
        /// @param _notifier 

        LimitsBin() : notifier(nullptr) {};
        LimitsBin(Notifier* _notifier): notifier(_notifier){};
        const unsigned int totalQty() const {return _totalQty; };
        void make(const BookEntry& makeEntry);
        void take(BookEntry& takeEntry);
        bool cancel(long ordId, unsigned int& remainingQty);

        bool hasDormantStops() const;
        void addDormantStop(const StopEntry& dormantStop);
        void moveAllStopsToActive(std::vector<StopEntry>& activeStops);
};
