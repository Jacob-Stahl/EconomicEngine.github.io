#include "matcher.h"

void Matcher::placeOrder(const Order& order){
    if(order.price < minPrice || order.price > (minPrice + priceRange)){
        throw new std::logic_error("order.price is outside of matcher price range");
    };

    notifier->registerOrder(order);

    // Place this order
    if(order.type == LIMIT){
        BookEntry entry{order};
        placeLimit(entry, order.side, order.price);
    }
    else if(order.type == MARKET){
        BookEntry entry{order};
        placeMarket(entry, order.side);
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

void Matcher::placeLimit(BookEntry& entry, Side side, int price){
    if(side == BUY){
        // try to match if it crosses the spread
        if(!spread.asksMissing && spread.lowestAsk <= price){
            takeSells(entry, price);
            if(entry.qty == 0){ return;}
        }

        // place on book
        auto& buyBin = getLimitsBin(price, buyLimitBins);
        buyBin.make(entry);

        // update spread        
        if(spread.bidsMissing || price > spread.highestBid){
            spread.highestBid = price;
        }
        spread.bidsMissing = false;
    }
    else{ // SELL
        if(!spread.bidsMissing && spread.highestBid >= price){
            takeBuys(entry, price);
            if(entry.qty == 0){ return;}
        }

        auto& sellBin = getLimitsBin(price, sellLimitBins);
        sellBin.make(entry);

        if(spread.asksMissing || price < spread.lowestAsk){
            spread.lowestAsk = price;
        }
        spread.asksMissing = false;
    }
    // TODO notify placement?
}

void Matcher::placeMarket(BookEntry& entry, Side side){
    bool takeSellLimits = side == BUY && !spread.asksMissing;
    bool takeBuyLimits = side == SELL && !spread.bidsMissing;
    if(takeSellLimits){
        takeSells(entry);
    }
    if(takeBuyLimits){
        takeBuys(entry);
    }

    // cancel what remains of this market order, if any
    if(entry.qty > 0){
        notifier->cancelled(entry.ordId, entry.qty);
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

inline size_t Matcher::priceToBinIdx(int price) const{
    return price - minPrice;
};

inline int Matcher::binIdxToPrice(size_t binIdx) const{
    return binIdx + minPrice;
} 

inline LimitsBin& Matcher::getLimitsBin(int price, std::vector<LimitsBin>& bins){
    size_t priceIdx = priceToBinIdx(price);
    return bins[priceIdx];
}

void Matcher::takeSells(BookEntry& buyOrder, int maxLimitPrice){
    size_t startIdx = priceToBinIdx(spread.lowestAsk);

    for(auto binIdx = startIdx; binIdx < sellLimitBins.size(); ++binIdx){
        int price = binIdxToPrice(binIdx);
        auto&& limitsBin = sellLimitBins[binIdx];

        if(limitsBin.hasDormantStops()){
            limitsBin.moveAllStopsToActive(activeBuyStops);
        }

        if(buyOrder.qty > 0 && price <= maxLimitPrice){
            limitsBin.take(buyOrder); // first fill the order
        }
        if(limitsBin.totalQty() == 0){
            continue; // then find a non-empty bin with the best asks
        }
        spread.asksMissing = false;
        spread.lowestAsk = price;
        return; // return after updating the spread
    }

    // If we reach this point, all ask liquidity has been drained
    spread.asksMissing = true;
}

void Matcher::takeBuys(BookEntry& sellOrder, int minLimitPrice){
    size_t startIdx = priceToBinIdx(spread.highestBid);

    for(auto binIdx = startIdx; binIdx != 0; --binIdx){
        int price = binIdxToPrice(binIdx);
        auto&& limitsBin = buyLimitBins[binIdx];

        if(limitsBin.hasDormantStops()){
            limitsBin.moveAllStopsToActive(activeSellStops);
        }
        if(sellOrder.qty > 0 && price >= minLimitPrice){
            limitsBin.take(sellOrder);
        }
        if(limitsBin.totalQty() == 0){
            continue;
        }
        
        spread.bidsMissing = false;
        spread.highestBid = price;
        return;
    }

    spread.bidsMissing = true;
}

void Matcher::cancelOrder(long ordId){
    Order doomedOrder;
    bool orderOnBook = notifier->getOrder(ordId, doomedOrder);
    if(!orderOnBook){
        return;
    }

    unsigned int remainingQty = 0;
    if(doomedOrder.side == BUY){
        auto& bin = buyLimitBins.at(priceToBinIdx(doomedOrder.price));
        bin.cancel(ordId, remainingQty);
    }
    else{
        auto& bin = sellLimitBins.at(priceToBinIdx(doomedOrder.price));
        bin.cancel(ordId, remainingQty);
    }

    notifier->cancelled(ordId, remainingQty);
}

const Depth Matcher::getDepth() const {
    Depth depth;

    // Bids: highest price first
    for (size_t binIdx = buyLimitBins.size() - 1; binIdx > 0; --binIdx) {
        auto&& bin = buyLimitBins[binIdx];
        if (bin.totalQty() > 0) {
            depth.bidBins.push_back({binIdxToPrice(binIdx), bin.totalQty()});
        }
    }

    // Asks: lowest price first
    for (size_t binIdx = 0; binIdx < sellLimitBins.size(); ++binIdx) {
        auto&& bin = sellLimitBins[binIdx];
        if (bin.totalQty() > 0) {
            depth.askBins.push_back({binIdxToPrice(binIdx), bin.totalQty()});
        }
    }

    return depth;
}