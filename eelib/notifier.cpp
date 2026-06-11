#include "notifier.h"


void Notifier::matchFound(Order& make, Order& take, std::uint32_t transferQty){

    if(take.side == BUY){
        matches.emplace_back(take, make, transferQty, make.price);
    } else {
        matches.emplace_back(make, take, transferQty, make.price);
    }
}

void Notifier::cancelled(std::int64_t ordId){
    cancellations.push_back(ordId);
}