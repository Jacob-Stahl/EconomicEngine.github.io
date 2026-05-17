#pragma once

#include "notifier.h"
#include "limits_bin.h"
#include <vector>
#include <set>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <flat_map>

struct PriceBin{
    int price = 0;
    unsigned int totalQty = 0;
};

struct Depth{
    std::vector<PriceBin> bidBins;
    std::vector<PriceBin> askBins;
};

class Matcher{
    private:

        int minPrice = -16384;
        unsigned int priceRange = 32768;

        // Limit orders by price
        std::vector<LimitsBin> buyLimitBins;
        std::vector<LimitsBin> sellLimitBins;

        // Active stop order are cleared and recursivally placed by placeOrder()
        std::vector<StopEntry> activeBuyStops;
        std::vector<StopEntry> activeSellStops;

        Spread spread;

        size_t priceToBinIdx(int price) const;
        int binIdxToPrice(size_t binIdx) const;
        LimitsBin& getLimitsBin(int price, std::vector<LimitsBin>& bins);
        void placeLimit(BookEntry& entry, Side side, int price);
        void placeMarket(BookEntry& entry, Side side);
        void placeStop(const Order& order);
        void takeSells(BookEntry& takeEntry, int maxPrice = INT_MAX);
        void takeBuys(BookEntry& takeEntry, int minPrice = INT_MIN);

    public:
        void placeOrder(const Order& order);
        void cancelOrder(long ordId);
        const Spread& getSpread() const {return spread; };
        const Depth getDepth() const;


        // Keep this public or use friends?
        std::unique_ptr<Notifier> notifier;

        Matcher(int minPrice_ = -16384, unsigned int priceRange_ = 32768): 
            minPrice(minPrice_), 
            priceRange(priceRange_),
            buyLimitBins(priceRange_),
            sellLimitBins(priceRange_)
        {
            notifier = std::make_unique<Notifier>();
            for(auto& b : buyLimitBins)  b.notifier = notifier.get();
            for(auto& b : sellLimitBins) b.notifier = notifier.get();
        };
};