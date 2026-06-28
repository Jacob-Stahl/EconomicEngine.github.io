#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <string>
#include <vector>

#include "order.h"
#include "matcher.h"

void benchmarkMatcher();

int main(){
    benchmarkMatcher();
}

class OrderFactory {
    long currentId = 1;
    std::mt19937 gen;
    std::uniform_int_distribution<unsigned int> qtyDist;
    std::normal_distribution<double> priceDist;
    std::normal_distribution<double> stopOffsetDist;
    std::discrete_distribution<size_t> sideDist;
    std::discrete_distribution<size_t> typeDist;
    double spreadFactor = 100.0;
    std::string asset;

private:

    Side pickSide(){
        size_t idx = sideDist(gen);
        return static_cast<Side>(idx + 1);
    }

    OrdType pickOrdType(){
        size_t idx = typeDist(gen);
        return static_cast<OrdType>(idx + 1);
    }
    

public:
    OrderFactory(const std::string& asset_ = "TEST")
        : asset(asset_),
          qtyDist(1, 100),
          priceDist(1000.0, 100.0),
          stopOffsetDist(30.0, 10.0),
          sideDist({1.0, 1.0}),
          typeDist({
            0.1,  // MARKET
            1.0, // LIMIT
            0.0,  // STOP
            0.0,  // STOPLIMIT
        })
    {
        std::random_device rd;
        gen = std::mt19937(rd());
    }

    Order randomOrder() {
        Side side = pickSide(); //weighted_random_enum<Side>({1.0, 1.0});
        OrdType type = pickOrdType();

        unsigned int qty = qtyDist(gen);
        double basePrice = priceDist(gen);
        double stopOffset = std::max(1.0, stopOffsetDist(gen));

        int price, stopPrice;
        if (side == BUY) {
            price     = static_cast<int>(basePrice - spreadFactor);
            stopPrice = static_cast<int>(price - stopOffset);
        } else {
            price     = static_cast<int>(basePrice + spreadFactor);
            stopPrice = static_cast<int>(price + stopOffset);
        }

        long id = currentId++;
        OrderBuilder b{};
        b.withAsset(asset).withTraderId(id).withOrdId(id);

        switch (type) {
            case MARKET:    b.market(side, qty);                      break;
            case LIMIT:     b.limit(side, price, qty);                break;
            case STOP:      b.stop(side, stopPrice, qty);             break;
            case STOPLIMIT: b.stopLimit(side, price, stopPrice, qty); break;
        }

        return b.build();
    }
};

void benchmarkMatcher(){
    Matcher matcher;
    OrderFactory factory{"TEST"};

    const int numOrders = 5'100'000;
    std::vector<Order> orders;
    orders.reserve(numOrders);

    for (int i = 0; i < numOrders; ++i) {
        orders.push_back(factory.randomOrder());
    }

    std::cout << "Generated " << numOrders << " orders. Running benchmark...\n";

    size_t processed = 0;
    auto lastPrint = std::chrono::steady_clock::now();

    bool cancelPrev = false;


    size_t ordIdx = 0;
    for (auto& order : orders) {

        // Place order
        matcher.placeOrder(order);
        ++processed;

        if(ordIdx > 100){
            if(cancelPrev){
                matcher.cancelOrder(orders[ordIdx - 4].ordId);
            }
            cancelPrev = !cancelPrev;
        }

        auto now = std::chrono::steady_clock::now();
        if (now - lastPrint >= std::chrono::seconds(1)) {
            const Depth depth = matcher.getDepth();
            const Spread& spread = matcher.getSpread();

            std::cout << processed << " orders processed"
                      << " | bid bins:" << depth.bidBins.size()
                      << " ask bins:" << depth.askBins.size()
                      << " | matches:" << matcher.notifier->matches.size()
                      << " | bid:";

            if (spread.bidsMissing) std::cout << "-";
            else                    std::cout << spread.highestBid;

            std::cout << " ask:";
            if (spread.asksMissing) std::cout << "-";
            else                    std::cout << spread.lowestAsk;

            std::cout << "\n";
            lastPrint = now;
        }

        ++ordIdx;
    }

    std::cout << "Done!\n"
              << "Matches found: " << matcher.notifier->matches.size() << "\n"
              << "Cancellations: " << matcher.notifier->cancellations.size() << "\n";
}
