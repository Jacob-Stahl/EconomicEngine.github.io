
#include <string>
#include <cstring>
#include "order.h"

bool Order::treatAsMarket(const Spread& spread) const{
    switch(type){
        case MARKET:
            return true;
        case LIMIT:
            return false;
        case STOPLIMIT:
            return false;
        case STOP : {

            /*
            https://www.onixs.biz/fix-dictionary/4.4/glossary.html#Stop:~:text=Stop-,A%20stop%20order%20to%20buy%20which%20becomes%20a%20market%20order%20when,stop%20price%20after%20the%20order%20is%20represented%20in%20the%20Trading%20Crowd.,-OrdType%20%3C40%3E      
            Treat buy-stop as a buy-market if marketPrice >= price
            Treat sell-stop as a sell-market if marketPrice <= price
            */
            if (side == BUY) {
                if (spread.asksMissing) return false;
                return spread.lowestAsk >= stopPrice;
            } else {
                if (spread.bidsMissing) return false;
                return spread.highestBid <= stopPrice;
            }
        }
    }
}

bool Order::treatAsLimit(const Spread& spread) const {
    switch(type){
        case MARKET:
            return false;
        case LIMIT:
            return true;
        case STOPLIMIT:
            if(side == BUY){
                if (spread.asksMissing) return false;
                return spread.lowestAsk >= stopPrice;
            } else {
                if (spread.bidsMissing) return false;
                return spread.highestBid <= stopPrice;
            }
        case STOP : {
            return false;
        }
    }
}


// Order 2
OrderBuilder& OrderBuilder::limit(Side side, int price, unsigned int qty){
    typeSet = true;

    order.type = LIMIT;
    order.side = side;
    order.price = price;
    order.qty = qty;
    order.timeInForce = TimeInForce::GTC;

    return *this;
}

OrderBuilder& OrderBuilder::stopLimit(Side side, int price, int stopPrice, unsigned int qty){
    if(price == stopPrice){
        throw std::logic_error("STOPLIMIT stop price can't be the same as the limit price. Place a LIMIT instead.");
    }

    if(side == BUY){
        if(stopPrice > price){
            throw std::logic_error("BUY STOPLIMIT stop price must be less than limit price");
        }
    }
    else{
        if(stopPrice < price){
            throw std::logic_error("SELL STOPLIMIT stop price must be greater than the limit price");
        }
    }
    typeSet = true;
    order.type = STOPLIMIT;
    order.side = side;
    order.price = price;
    order.stopPrice = stopPrice;
    order.qty = qty;
    order.timeInForce = TimeInForce::GTC;

    return *this;
}

OrderBuilder& OrderBuilder::market(Side side, unsigned int qty){
    typeSet = true;
    order.type = MARKET;
    order.side = side;
    order.qty = qty;
    order.timeInForce = TimeInForce::IOC;

    return *this;
}

OrderBuilder& OrderBuilder::stop(Side side, int stopPrice, unsigned int qty){
    typeSet = true;
    order.type = STOP;
    order.side = side;
    order.stopPrice = stopPrice;
    order.qty = qty;
    order.timeInForce = TimeInForce::IOC;

    return *this;
}

Order2 OrderBuilder::build(){

    if(!typeSet){
        throw std::logic_error("Order type not set!");
    }

    if(!assetSet){
        throw std::logic_error("asset must be set!");
    }

    if(!traderIdSet){
        throw std::logic_error("traderId must be set!");
    }

    if(!ordIdSet){
        throw std::logic_error("ordId must be set!");
    }

    if(order.qty == 0){
        throw std::logic_error("Order qty must be > 0");
    };

    return order;
}

OrderBuilder& OrderBuilder::withAsset(const std::string& asset){

    if(asset.length() > 15){
        throw std::logic_error("Asset name must be 15 characters of less");
    }

    assetSet = true;
    strcpy(order.asset, asset.c_str());
    return *this;
}

OrderBuilder& OrderBuilder::withTraderId(long traderId){
    traderIdSet = true;
    order.traderId = traderId;
    return *this;
}

OrderBuilder& OrderBuilder::withOrdId(long ordId){
    ordIdSet = true;
    order.ordId = ordId;
    return *this;
}