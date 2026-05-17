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
        Match(const Order& ord1, const Order& ord2, long qty_, int price_)
        {
            if (ord1.side == ord2.side) { std::logic_error("Can't match orders on the same side!"); }
            if (ord1.asset != ord2.asset) { std::logic_error("Can't match orders with different assets!"); } // TODO add test for this
            if (ord1.side == BUY){
                buyer = ord1;
                seller = ord2;
            }
            else{
                buyer = ord2;
                seller = ord1;
            }

            qty = qty_;
            price = price_;
        }

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



struct Match2{
    Order2 buyer;
    Order2 seller;
    long qty;
    int price;
    

    public:
        Match2(const Order2& ord1, const Order2& ord2, long qty_, int price_)
        {
            if (ord1.side == ord2.side) { std::logic_error("Can't match orders on the same side!"); }
            if (ord1.side == BUY){
                buyer = ord1;
                seller = ord2;
            }
            else{
                buyer = ord2;
                seller = ord1;
            }

            qty = qty_;
            price = price_;
        }

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