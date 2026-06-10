#pragma once

#include "order.h"
#include "notifier.h"
#include <queue>
#include <unordered_set>

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

class LimitsBin{
    private:
        std::vector<StopEntry> dormantStops;
        std::unordered_set<std::uint64_t> cancelledIds; // TODO: consider using pool allocator here
        std::deque<BookEntry> entries;
        void notifyMatch(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty);
        bool findEraseCancelledLimit(std::int64_t orderId);

    public:

        Notifier* notifier = nullptr;

        /// @brief Takes raw pointer to notifier in Matcher. LimitsBin has no effect on the nofifier lifetime
        /// @param _notifier 

        LimitsBin() : notifier(nullptr) {};
        LimitsBin(Notifier* _notifier): notifier(_notifier){};
        std::uint32_t totalQty();
        inline bool isEmpty() const {return entries.size() == 0;}
        void make(const BookEntry& makeEntry);
        void take(BookEntry& takeEntry);
        void cancelLimit(std::int64_t ordId);
        void cancelStop(std::int64_t ordId); // TODO: add test coverage for stop cancellation

        bool hasDormantStops() const;
        void addDormantStop(const StopEntry& dormantStop);
        void moveAllStopsToActive(std::vector<StopEntry>& activeStops);
};
