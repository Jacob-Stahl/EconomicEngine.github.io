#include "matcher.h"

void Matcher::placeOrder(Order order){
    if(order.price < minPrice || order.price > (minPrice + (std::int32_t)priceRange)){
        throw new std::logic_error("order.price is outside of matcher price range");
    };

    if(order.type != MARKET){
        orderRegistry.insert_or_assign(order.ordId, order);
    }

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
    // Stops can trigger a chain reaction, requiring a loop
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

    // Market order do not stay on the book.
    orderRegistry.erase(marketOrd.ordId);
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

void Matcher::refreshHighestBid(size_t startIdx){
    if(buyLimitBins.empty()){
        spread.bidsMissing = true;
        return;
    }

    for(size_t binIdx = std::min(startIdx, buyLimitBins.size() - 1) + 1; binIdx-- > 0;){
        if(buyLimitBins[binIdx].totalQty() == 0){
            continue;
        }

        spread.bidsMissing = false;
        spread.highestBid = binIdxToPrice(binIdx);
        return;
    }

    spread.bidsMissing = true;
}

void Matcher::refreshLowestAsk(size_t startIdx){
    for(size_t binIdx = startIdx; binIdx < sellLimitBins.size(); ++binIdx){
        if(sellLimitBins[binIdx].totalQty() == 0){
            continue;
        }

        spread.asksMissing = false;
        spread.lowestAsk = binIdxToPrice(binIdx);
        return;
    }

    spread.asksMissing = true;
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
            limitsBin.take(buyOrder, orderRegistry); // first fill the order
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

    for(size_t binIdx = startIdx + 1; binIdx-- > 0;){
        std::int32_t price = binIdxToPrice(binIdx);
        auto&& limitsBin = buyLimitBins[binIdx];

        if(limitsBin.hasDormantStops()){
            limitsBin.moveAllStopsToActive(activeSellStops);
        }

        if(sellOrder.qty > 0 && price >= minLimitPrice){
            limitsBin.take(sellOrder, orderRegistry);
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
    auto it = orderRegistry.find(ordId);
    if(it == orderRegistry.end()){
        return;
    }
    const Order& doomedOrder = it->second;


    // If this is a STOP or STOPLIMIT order, find the correct bin cancel the dormant stop entry.
    if(doomedOrder.type == STOP || doomedOrder.type == STOPLIMIT){
        LimitsBin& bin = doomedOrder.side == BUY ?
            sellLimitBins.at(priceToBinIdx(doomedOrder.stopPrice)) :
            buyLimitBins.at(priceToBinIdx(doomedOrder.stopPrice));

        if(bin.cancelStop(ordId)){
            orderRegistry.erase(it);
            return;
        }
    }

    // If this is a LIMIT or STOPLIMIT, find the correct bin and cancel the limit entry
    if(doomedOrder.type == LIMIT || doomedOrder.type == STOPLIMIT){
        LimitsBin& bin = doomedOrder.side == BUY ?
            buyLimitBins.at(priceToBinIdx(doomedOrder.price)) :
            sellLimitBins.at(priceToBinIdx(doomedOrder.price));

        if(!bin.cancelLimit(ordId)){
            return;
        }

        // Refresh the spread since the LOB was modified
        if(doomedOrder.side == BUY &&
           !spread.bidsMissing &&
           doomedOrder.price == spread.highestBid &&
           bin.totalQty() == 0){
            refreshHighestBid(priceToBinIdx(doomedOrder.price));
        }
        else if(doomedOrder.side == SELL &&
                !spread.asksMissing &&
                doomedOrder.price == spread.lowestAsk &&
                bin.totalQty() == 0){
            refreshLowestAsk(priceToBinIdx(doomedOrder.price));
        }
    }

    orderRegistry.erase(it);
}

const Depth Matcher::getDepth() {
    Depth depth;

    // Bids: highest price first
    for (size_t binIdx = buyLimitBins.size(); binIdx-- > 0;) {
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