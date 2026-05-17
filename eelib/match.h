#pragma once

#include <vector>
#include "order.h"
#include <stdexcept>

struct Match{
    Order buyer;
    Order seller;
    long qty;
    int price;
    

    public:
        Match(const Order& buyer_, const Order& seller_, long qty_, int price_):
            buyer(buyer_),
            seller(seller_),
            qty(qty_),
            price(price_){}

        long cashTransfered() const{

            if(buyer.type == LIMIT || buyer.type == STOPLIMIT){
                return (long)price * qty;
            }
            else if (seller.type == LIMIT || seller.type == STOPLIMIT){
                return (long)price * qty;
            }
            
            throw std::logic_error("Can't determine cashTransfered with thr provided order types!");
        }
};