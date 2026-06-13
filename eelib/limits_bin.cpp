#include "limits_bin.h"
#include <algorithm>

void LimitsBin::make(Order& makeEntry){
    entries.push_back(makeEntry);
}

void LimitsBin::take(Order& takeEntry, std::unordered_map<std::int64_t, Order>& orderRegistry){
    while (takeEntry.qty > 0 && !entries.empty()) {
        auto& makeEntry = entries.front();

        // Skip and remove, if this order is cancelled
        if(cancelledIds.erase(makeEntry.ordId) > 0){
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
            orderRegistry.erase(makeEntry.ordId);
            entries.pop_front();
        }
    };

    if (takeEntry.qty == 0){
        orderRegistry.erase(takeEntry.ordId);
    }
}

std::uint32_t LimitsBin::totalQty(){
    std::uint32_t total = 0;
    for(auto& entry : entries){
        if(entry.qty == 0 || cancelledIds.count(entry.ordId)){
            continue;
        }
        total += entry.qty;
    }

    return total;
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