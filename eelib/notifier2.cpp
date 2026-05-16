#include "notifier2.h"

bool Notifier2::getOrder(long ordId, Order2& order) const{
    auto registryItem = orderRegistery.find(ordId);
    if(registryItem == orderRegistery.end()){
        return false;
    }
    else{
        order = registryItem->second;
        return true; 
    }
};

void Notifier2::matchFound(long makeId, long takeId, unsigned int transferQty){
    const Order2& make = orderRegistery.at(makeId);
    const Order2& take = orderRegistery.at(takeId);

    if(take.side == BUY){
        matches.emplace_back(take, make, transferQty, make.price);
    } else {
        matches.emplace_back(make, take, transferQty, make.price);
    }
}

void Notifier2::cancelled(long ordId, unsigned int remainingQty){
    Order2 cancelledOrder;  
    getOrder(ordId, cancelledOrder);
    cancellations.push_back(cancelledOrder);
    orderRegistery.erase(ordId);
}