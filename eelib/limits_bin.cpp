#include "limits_bin.h"
#include <algorithm>

void LimitsBin::make(const BookEntry& makeEntry){
    entries.push_back(makeEntry);
    _totalQty += makeEntry.qty;
}

void LimitsBin::take(BookEntry& takeEntry){
    while (takeEntry.qty > 0 && !entries.empty()) {
        auto& makeEntry = entries.front();

        // Skip and remove cancelled orders;
        if(makeEntry.isCancelled){
            entries.pop_front();
            continue;
        }

        // transfer matching qty
        std::uint32_t transferQty = std::min(takeEntry.qty, makeEntry.qty);
        takeEntry.qty -= transferQty;
        makeEntry.qty -= transferQty;
        _totalQty -= transferQty;

        // send match notification
        notifyMatch(makeEntry.ordId, takeEntry.ordId, transferQty);

        // remove order on book if its empty
        if(makeEntry.qty == 0){
            entries.pop_front();
        }
    };
}

bool LimitsBin::cancelLimit(std::int64_t ordId){
    for(auto& order : entries){
        if(order.ordId == ordId){
            order.isCancelled = true;
            _totalQty -= order.qty;
            return true;
        }
    }
    return false;
}

bool LimitsBin::cancelStop(std::int64_t ordId){
    for(auto& stop : dormantStops){
        if(stop.entry.ordId == ordId){
            stop.entry.isCancelled = true;
            return true;
        }
    } 
    return false;
}

void LimitsBin::notifyMatch(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty){
    notifier->matchFound(makeId, takeId, transferQty);
}

void LimitsBin::addDormantStop(const StopEntry& dormantStop){
    dormantStops.push_back(dormantStop);
}

void LimitsBin::moveAllStopsToActive(std::vector<StopEntry>& activeStops){
    for(auto& stop : dormantStops){
        if(!stop.entry.isCancelled){
            activeStops.push_back(std::move(stop));
        }
    }
    dormantStops.clear();
}

bool LimitsBin::hasDormantStops() const{
    return !dormantStops.empty();
}