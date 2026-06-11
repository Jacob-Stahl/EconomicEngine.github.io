#include "limits_bin.h"
#include <algorithm>

void LimitsBin::make(Order& makeEntry){
    entries.push_back(makeEntry);
}

void LimitsBin::take(Order& takeEntry){
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

        // send match notification
        notifier->matchFound(makeEntry, takeEntry, transferQty);

        // remove order on book if its empty
        if(makeEntry.qty == 0){
            entries.pop_front();
        }
    };
}

std::uint32_t LimitsBin::totalQty(){
    std::uint32_t total = 0;
    for(auto& entry : entries){
        if(entry.qty == 0 || findEraseCancelledLimit(entry.ordId)){
            entries.pop_front();
            continue;
        }
        total += entry.qty;
    }

    return total;
}

inline bool LimitsBin::findEraseCancelledLimit(std::int64_t ordId){
    return cancelledIds.erase(ordId) > 0;
}

void LimitsBin::cancelLimit(std::int64_t ordId){
    cancelledIds.insert(ordId);
    notifier->cancelled(ordId);
    return;
}

void LimitsBin::cancelStop(std::int64_t ordId){
    for(auto& stop : dormantStops){
        if(stop.entry.ordId == ordId){
            stop.isCancelled = true;
            notifier->cancelled(ordId);
            break;
        }
    } 
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

bool LimitsBin::isEmpty() const{
    return entries.size() - cancelledIds.size() <= 0;
}