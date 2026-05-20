
#include <string>
#include <cstring>
#include "order.h"

OrderBuilder& OrderBuilder::limit(Side side, std::int32_t price, std::uint32_t qty){
    typeSet = true;

    order.type = LIMIT;
    order.side = side;
    order.price = price;
    order.qty = qty;
    order.timeInForce = TimeInForce::GTC;

    return *this;
}

OrderBuilder& OrderBuilder::stopLimit(Side side, std::int32_t price, std::int32_t stopPrice, std::uint32_t qty){
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

OrderBuilder& OrderBuilder::market(Side side, std::uint32_t qty){
    typeSet = true;
    order.type = MARKET;
    order.side = side;
    order.qty = qty;
    order.timeInForce = TimeInForce::IOC;

    return *this;
}

OrderBuilder& OrderBuilder::stop(Side side, std::int32_t stopPrice, std::uint32_t qty){
    typeSet = true;
    order.type = STOP;
    order.side = side;
    order.stopPrice = stopPrice;
    order.qty = qty;
    order.timeInForce = TimeInForce::IOC;

    return *this;
}

Order OrderBuilder::build(){

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

    auto builtOrder = std::move(order);
    order = Order{};
    return builtOrder;
}

OrderBuilder& OrderBuilder::withAsset(const std::string& asset){

    if(asset.length() > 15){
        throw std::logic_error("Asset name must be 15 characters of less");
    }

    assetSet = true;
    strcpy(order.asset, asset.c_str());
    return *this;
}

OrderBuilder& OrderBuilder::withTraderId(std::int64_t traderId){
    traderIdSet = true;
    order.traderId = traderId;
    return *this;
}

OrderBuilder& OrderBuilder::withOrdId(std::int64_t ordId){
    ordIdSet = true;
    order.ordId = ordId;
    return *this;
}