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

// weighted picker: weights.size() == number of enum values (ordered by underlying value starting at 1)
template<typename Enum>
Enum weighted_random_enum(const std::vector<double>& weights) {
    static_assert(std::is_enum_v<Enum>);
    static std::mt19937 rng{std::random_device{}()};
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    size_t idx = dist(rng);
    using U = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<U>(idx + 1)); // +1 because enums start at 1
}

class OrderFactory {
    long currentId = 1;
    std::mt19937 gen;
    std::uniform_int_distribution<unsigned int> qtyDist;
    std::normal_distribution<double> priceDist;
    std::normal_distribution<double> stopOffsetDist;
    double spreadFactor = 100.0;
    std::string asset;

public:
    OrderFactory(const std::string& asset_ = "TEST")
        : asset(asset_),
          qtyDist(1, 100),
          priceDist(1000.0, 100.0),
          stopOffsetDist(30.0, 10.0)
    {
        std::random_device rd;
        gen = std::mt19937(rd());
    }

    Order randomOrder() {
        Side side = weighted_random_enum<Side>({1.0, 1.0});
        OrdType type = weighted_random_enum<OrdType>({
            0.1,  // MARKET
            1.0, // LIMIT
            0.1,  // STOP
            0.1,  // STOPLIMIT
        });

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
