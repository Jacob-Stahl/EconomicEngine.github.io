#pragma once

#include <vector>
#include <stdexcept>

#include "order.h"

struct Match{
    Order buyer;
    Order seller;
    std::uint32_t qty;
    std::int32_t price;
    
    public:
        Match(const Order& buyer_, const Order& seller_, std::uint32_t qty_, std::int32_t price_):
            buyer(buyer_),
            seller(seller_),
            qty(qty_),
            price(price_){}

        std::int64_t cashTransfered() const{

            if(buyer.type == LIMIT || buyer.type == STOPLIMIT){
                return (std::int64_t)price * qty;
            }
            else if (seller.type == LIMIT || seller.type == STOPLIMIT){
                return (std::int64_t)price * qty;
            }
            
            throw std::logic_error("Can't determine cashTransfered with thr provided order types!");
        }
};