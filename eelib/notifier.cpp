#include "notifier.h"

bool Notifier::getOrder(std::int64_t ordId, Order& order) const{
    auto registryItem = orderRegistery.find(ordId);
    if(registryItem == orderRegistery.end()){
        return false;
    }
    else{
        order = registryItem->second;
        return true; 
    }
};

void Notifier::matchFound(std::int64_t makeId, std::int64_t takeId, std::uint32_t transferQty){
    const Order& make = orderRegistery.at(makeId);
    const Order& take = orderRegistery.at(takeId);

    if(take.side == BUY){
        matches.emplace_back(take, make, transferQty, make.price);
    } else {
        matches.emplace_back(make, take, transferQty, make.price);
    }
}

void Notifier::cancelled(std::int64_t ordId){
    Order cancelledOrder;  
    getOrder(ordId, cancelledOrder);
    cancellations.push_back(cancelledOrder);
    //orderRegistery.erase(ordId);
}