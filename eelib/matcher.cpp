#include "matcher.h"

void Matcher::placeOrder(Order& order){
    if(order.price < minPrice || order.price > (minPrice + (std::int32_t)priceRange)){
        throw new std::logic_error("order.price is outside of matcher price range");
    };

    //notifier->registerOrder(order);

    // Place this order
    if(order.type == LIMIT){
        placeLimit(order, order.side, order.price);
    }
    else if(order.type == MARKET){
        placeMarket(order, order.side);
    }
    else if(order.type == STOPLIMIT || order.type == STOP){
        placeStop(order);
    }

    // If this order activates any stops, place them on the book
    while(!activeBuyStops.empty() || !activeSellStops.empty()){
        auto sellBatch = std::move(activeSellStops); // activeSellStops is now empty
        for(auto& stopEntry : sellBatch){
            if(stopEntry.type == STOPLIMIT) placeLimit(stopEntry.entry, SELL, stopEntry.limitPrice);
            else if(stopEntry.type == STOP) placeMarket(stopEntry.entry, SELL);
        }
        auto buyBatch = std::move(activeBuyStops);
        for(auto& stopEntry : buyBatch){
            if(stopEntry.type == STOPLIMIT) placeLimit(stopEntry.entry, BUY, stopEntry.limitPrice);
            else if(stopEntry.type == STOP) placeMarket(stopEntry.entry, BUY);
        }
    }
}

void Matcher::placeLimit(Order& limitOrd, Side side, std::int32_t price){
    if(side == BUY){
        // try to match if it crosses the spread
        if(!spread.asksMissing && spread.lowestAsk <= price){
            takeSells(limitOrd, price);
            if(limitOrd.qty == 0){ return;}
        }

        // place on book
        auto& buyBin = getLimitsBin(price, buyLimitBins);
        buyBin.make(limitOrd);

        // update spread        
        if(spread.bidsMissing || price > spread.highestBid){
            spread.highestBid = price;
        }
        spread.bidsMissing = false;
    }
    else{ // SELL
        if(!spread.bidsMissing && spread.highestBid >= price){
            takeBuys(limitOrd, price);
            if(limitOrd.qty == 0){ return;}
        }

        auto& sellBin = getLimitsBin(price, sellLimitBins);
        sellBin.make(limitOrd);

        if(spread.asksMissing || price < spread.lowestAsk){
            spread.lowestAsk = price;
        }
        spread.asksMissing = false;
    }
    // TODO notify placement?
}

void Matcher::placeMarket(Order& marketOrd, Side side){
    bool takeSellLimits = side == BUY && !spread.asksMissing;
    bool takeBuyLimits = side == SELL && !spread.bidsMissing;
    if(takeSellLimits){
        takeSells(marketOrd);
    }
    if(takeBuyLimits){
        takeBuys(marketOrd);
    }

    // cancel what remains of this market order, if any
    if(marketOrd.qty > 0){
        notifier->cancelled(marketOrd.ordId);
    }
}

// https://www.interactivebrokers.com.hk/php/webhelp/Making_Trades/trigger.htm
// https://money.stackexchange.com/questions/145433/sell-stop-limit-triggered-on-bid-or-ask
void Matcher::placeStop(const Order& order){
    StopEntry dormantStop(order);

    // Put stops in trigger price bins if the are NOT active on placement
    bool buyStopDormant = order.side == BUY && (order.stopPrice > spread.lowestAsk);
    bool sellStopDormant = order.side == SELL && (order.stopPrice < spread.highestBid);
    if(buyStopDormant){
        auto& bin = getLimitsBin(order.stopPrice, sellLimitBins);
        bin.addDormantStop(dormantStop);
        return;
    }
    if(sellStopDormant){
        auto& bin = getLimitsBin(order.stopPrice, buyLimitBins);
        bin.addDormantStop(dormantStop);
        return;
    }

    // Place stops on book if they are active on placement.
    if(order.type == STOPLIMIT) placeLimit(dormantStop.entry, order.side, order.price);
    if(order.type == STOP) placeMarket(dormantStop.entry, order.side);
}

inline size_t Matcher::priceToBinIdx(std::int32_t price) const{
    return price - minPrice;
};

inline std::int32_t Matcher::binIdxToPrice(size_t binIdx) const{
    return binIdx + minPrice;
} 

inline LimitsBin& Matcher::getLimitsBin(std::int32_t price, std::vector<LimitsBin>& bins){
    size_t priceIdx = priceToBinIdx(price);
    return bins[priceIdx];
}

void Matcher::takeSells(Order& buyOrder, std::int32_t maxLimitPrice){
    size_t startIdx = priceToBinIdx(spread.lowestAsk);

    for(auto binIdx = startIdx; binIdx < sellLimitBins.size(); ++binIdx){
        std::int32_t price = binIdxToPrice(binIdx);
        auto&& limitsBin = sellLimitBins[binIdx];

        if(limitsBin.hasDormantStops()){
            limitsBin.moveAllStopsToActive(activeBuyStops);
        }

        if(buyOrder.qty > 0 && price <= maxLimitPrice){
            limitsBin.take(buyOrder); // first fill the order
        }
        if(limitsBin.isEmpty()){
            continue; // then find a non-empty bin with the best asks
        }
        spread.asksMissing = false;
        spread.lowestAsk = price;
        return; // return after updating the spread
    }

    // If we reach this point, all ask liquidity has been drained
    spread.asksMissing = true;
}

void Matcher::takeBuys(Order& sellOrder, std::int32_t minLimitPrice){
    size_t startIdx = priceToBinIdx(spread.highestBid);

    for(auto binIdx = startIdx; binIdx != 0; --binIdx){
        std::int32_t price = binIdxToPrice(binIdx);
        auto&& limitsBin = buyLimitBins[binIdx];

        if(limitsBin.hasDormantStops()){
            limitsBin.moveAllStopsToActive(activeSellStops);
        }
        if(sellOrder.qty > 0 && price >= minLimitPrice){
            limitsBin.take(sellOrder);
        }
        if(limitsBin.isEmpty()){
            continue;
        }
        
        spread.bidsMissing = false;
        spread.highestBid = price;
        return;
    }

    spread.bidsMissing = true;
}

void Matcher::cancelOrder(std::int64_t ordId){
    Order doomedOrder;
    
    // We are no longer checking if the order is on the book
    //bool orderOnBook = notifier->getOrder(ordId, doomedOrder);
    //if(!orderOnBook){
    //    return;
    //}

    // Cancel Limits
    LimitsBin& bin = doomedOrder.side == BUY ? 
        buyLimitBins.at(priceToBinIdx(doomedOrder.price)):
        sellLimitBins.at(priceToBinIdx(doomedOrder.price));
    bin.cancelLimit(ordId);

    // If this is a STOP order, and it was not found in active limits, check dormant stops
    if(doomedOrder.type == STOP || doomedOrder.type == STOPLIMIT){
        LimitsBin& bin = doomedOrder.side == BUY ? 
            buyLimitBins.at(priceToBinIdx(doomedOrder.stopPrice)):
            sellLimitBins.at(priceToBinIdx(doomedOrder.stopPrice));
        bin.cancelStop(ordId);
    }
}

const Depth Matcher::getDepth() {
    Depth depth;

    // Bids: highest price first
    for (size_t binIdx = buyLimitBins.size() - 1; binIdx > 0; --binIdx) {
        auto binQty = buyLimitBins[binIdx].totalQty();
        if (binQty > 0) {
            depth.bidBins.push_back({binIdxToPrice(binIdx), binQty});
        }
    }

    // Asks: lowest price first
    for (size_t binIdx = 0; binIdx < sellLimitBins.size(); ++binIdx) {
        auto binQty = sellLimitBins[binIdx].totalQty();
        if (binQty > 0) {
            depth.askBins.push_back({binIdxToPrice(binIdx), binQty});
        }
    }

    return depth;
}