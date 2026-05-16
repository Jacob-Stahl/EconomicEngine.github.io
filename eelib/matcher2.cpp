#include "matcher2.h"

void Matcher2::placeOrder(const Order2& order){
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

void Matcher2::placeLimit(BookEntry& entry, Side side, int price){
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

void Matcher2::placeMarket(BookEntry& entry, Side side){
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
void Matcher2::placeStop(const Order2& order){
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

inline LimitsBin& Matcher2::getLimitsBin(int price, std::map<int, LimitsBin>& bins){
    auto [it, _] = bins.try_emplace(price, notifier.get());
    return it->second;
}

void Matcher2::takeSells(BookEntry& buyOrder, int maxPrice){
    // Iterating flat maps is a bit different than regular maps:
    // https://stackoverflow.com/questions/79847808/how-can-i-iterate-a-flat-map-in-a-range-based-for-loop-updating-values
    // https://stackoverflow.com/questions/13230480/what-is-the-meaning-of-a-variable-with-type-auto
    for(auto&& [price, bin] : sellLimitBins){
        if(bin.hasDormantStops()){
            bin.moveAllStopsToActive(activeBuyStops);
        }

        if(buyOrder.qty > 0 && price <= maxPrice){
            bin.take(buyOrder); // first fill the order
        }
        if(bin.totalQty() == 0){
            continue; // then find a non-empty bin with the best asks
        }
        spread.asksMissing = false;
        spread.lowestAsk = price;
        return; // return after updating the spread
    }

    // If we reach this point, all ask liquidity has been drained
    spread.asksMissing = true;
}

void Matcher2::takeBuys(BookEntry& sellOrder, int minPrice){
    for(auto bin = buyLimitBins.rbegin(); bin != buyLimitBins.rend(); bin++){
        auto&& [price, limitsBin] = *bin;

        if(limitsBin.hasDormantStops()){
            limitsBin.moveAllStopsToActive(activeSellStops);
        }
        if(sellOrder.qty > 0 && price >= minPrice){
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

void Matcher2::cancelOrder(long ordId){
    Order2 doomedOrder;
    bool orderOnBook = notifier->getOrder(ordId, doomedOrder);
    if(!orderOnBook){
        return;
    }

    unsigned int remainingQty = 0;
    if(doomedOrder.side == BUY){
        auto& bin = buyLimitBins.at(doomedOrder.price);
        bin.cancel(ordId, remainingQty);
    }
    else{
        auto& bin = sellLimitBins.at(doomedOrder.price);
        bin.cancel(ordId, remainingQty);
    }

    notifier->cancelled(ordId, remainingQty);
}

const Depth Matcher2::getDepth() const {
    Depth depth;

    // Bids: highest price first
    for (auto it = buyLimitBins.rbegin(); it != buyLimitBins.rend(); ++it) {
        if (it->second.totalQty() > 0) {
            depth.bidBins.push_back({it->first, it->second.totalQty()});
        }
    }

    // Asks: lowest price first
    for (auto it = sellLimitBins.begin(); it != sellLimitBins.end(); ++it) {
        if (it->second.totalQty() > 0) {
            depth.askBins.push_back({it->first, it->second.totalQty()});
        }
    }

    return depth;
}