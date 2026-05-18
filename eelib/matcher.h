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
    std::int32_t price = 0;
    std::uint32_t  totalQty = 0;
};

struct Depth{
    std::vector<PriceBin> bidBins;
    std::vector<PriceBin> askBins;
};

class Matcher{
    private:

        std::int32_t minPrice = -16384;
        std::uint32_t priceRange = 32768;

        // Limit orders by price
        std::vector<LimitsBin> buyLimitBins;
        std::vector<LimitsBin> sellLimitBins;

        // Active stop order are cleared and recursivally placed by placeOrder()
        std::vector<StopEntry> activeBuyStops;
        std::vector<StopEntry> activeSellStops;

        Spread spread;

        size_t priceToBinIdx(std::int32_t price) const;
        std::int32_t binIdxToPrice(size_t binIdx) const;
        LimitsBin& getLimitsBin(std::int32_t price, std::vector<LimitsBin>& bins);
        void placeLimit(BookEntry& entry, Side side, std::int32_t price);
        void placeMarket(BookEntry& entry, Side side);
        void placeStop(const Order& order);
        void takeSells(BookEntry& takeEntry, std::int32_t maxPrice = INT32_MAX);
        void takeBuys(BookEntry& takeEntry, std::int32_t minPrice = INT32_MIN);

    public:
        void placeOrder(const Order& order);
        void cancelOrder(std::int64_t ordId);
        const Spread& getSpread() const {return spread; };
        const Depth getDepth() const;


        // Keep this public or use friends?
        std::unique_ptr<Notifier> notifier;

        Matcher(std::int32_t minPrice_ = -16384, std::uint32_t priceRange_ = 32768): 
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