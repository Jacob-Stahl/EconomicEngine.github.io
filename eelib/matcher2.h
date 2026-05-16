#pragma once

#include "notifier2.h"
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

class Matcher2{
    private:
        // Limit orders by price
        std::flat_map<int, LimitsBin> buyLimitBins;
        std::flat_map<int, LimitsBin> sellLimitBins;

        // Active stop order are cleared and recursivally placed by placeOrder()
        std::vector<StopEntry> activeBuyStops;
        std::vector<StopEntry> activeSellStops;

        Spread spread;

        LimitsBin& getLimitsBin(int price, std::flat_map<int, LimitsBin>& bins);
        void placeLimit(BookEntry& entry, Side side, int price);
        void placeMarket(BookEntry& entry, Side side);
        void placeStop(const Order2& order);
        void takeSells(BookEntry& takeEntry, int maxPrice = INT_MAX);
        void takeBuys(BookEntry& takeEntry, int minPrice = INT_MIN);

    public:
        void placeOrder(const Order2& order);
        void cancelOrder(long ordId);
        const Spread& getSpread() const {return spread; };
        const Depth getDepth() const;


        // Keep this public or use friends?
        std::unique_ptr<Notifier2> notifier;

        Matcher2(){
            notifier = std::make_unique<Notifier2>();
        };
};