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
        if(findEraseCancelledLimit(makeEntry.ordId)){
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

inline bool LimitsBin::findEraseCancelledLimit(std::int64_t orderId){
    if(numCancelledLimits == 0) return false;

    for(auto& cancelledLim : cancelledLimits){
        if(orderId == cancelledLim){
            cancelledLim = -1;
            --numCancelledLimits;
            return true;
        }
    }
    return false;
}

void LimitsBin::cancelLimit(std::int64_t ordId){
    if(numCancelledLimits == cancelledLimits.size()){
        cancelledLimits.push_back(ordId);
    }
    else{
        for(auto& slot : cancelledLimits){
            if(slot == -1){
                slot = ordId;
            }
        }
    }

    ++numCancelledLimits;
    notifier->cancelled(ordId);
    return;
}

void LimitsBin::cancelStop(std::int64_t ordId){
    for(auto& stop : dormantStops){
        if(stop.entry.ordId == ordId){
            stop.isCancelled = true;
            notifier->cancelled(ordId);
        }
    } 
}

void LimitsBin::notifyMatch(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty){
    notifier->matchFound(makeId, takeId, transferQty);
}

void LimitsBin::addDormantStop(const StopEntry& dormantStop){
    dormantStops.push_back(dormantStop);
}

void LimitsBin::moveAllStopsToActive(std::vector<StopEntry>& activeStops){
    for(auto& stop : dormantStops){
        if(!stop.isCancelled){
            activeStops.push_back(std::move(stop));
        }
    }
    dormantStops.clear();
}

bool LimitsBin::hasDormantStops() const{
    return !dormantStops.empty();
}