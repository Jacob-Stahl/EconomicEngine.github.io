#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <string>

#include "order.h"
#include "matcher.h"
#include "agent.h"
#include "abm.h"
#include "agent_manager.h"

void benchmarkMatcher();
void tinkerWithABM_ConsumptionEconV1();
void tinkerWithABM_ConsumptionEconV2();

int main() {
    tinkerWithABM_ConsumptionEconV1();
}


template <typename Enum, int maxValue>
Enum random_enum()
{
    static_assert(std::is_enum_v<Enum>);

    static std::mt19937 rng{std::random_device{}()};

    using U = std::underlying_type_t<Enum>;

    constexpr U min = static_cast<U>(1);
    constexpr U max = static_cast<U>(maxValue);

    std::uniform_int_distribution<U> dist(min, max);
    return static_cast<Enum>(dist(rng));
}

// weighted picker: weights.size() == number of enum values (ordered by underlying value starting at 1)
template<typename Enum>
Enum weighted_random_enum(const std::vector<double>& weights) {
    static_assert(std::is_enum_v<Enum>);
    static std::mt19937 rng{std::random_device{}()};
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    size_t idx = dist(rng);           // idx in [0, weights.size()-1]
    using U = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<U>(idx + 1)); // +1 because enums are assumed to start at 1
}

class OrderFactory{
    long int currentIdTimestamp = 1;
    std::mt19937 gen;
    std::uniform_int_distribution<long> qtyDistrib;
    std::normal_distribution<double> priceDistrib;
    std::normal_distribution<double> stopPriceFactorDistrib;
    double spreadFactor = 10;

    public:
        OrderFactory(){
            qtyDistrib = std::uniform_int_distribution<long>(1, 100);
            priceDistrib = std::normal_distribution<double>(1000, 100);
            stopPriceFactorDistrib = std::normal_distribution<double>(30, 10);
            
            std::random_device device;
            gen = std::mt19937(device());
        }

        Order newOrder(Side side, OrdType type, long qty, long price = 0, long stopPrice = 0){
            Order o{};
            o.traderId = currentIdTimestamp;
            o.ordId = currentIdTimestamp;
            o.side = side;
            o.qty = qty;
            o.price = price;
            o.stopPrice = stopPrice;
            o.asset = "TEST";
            o.type = type;
            o.ordNum = currentIdTimestamp;

            ++currentIdTimestamp;
            return o;
        }

        Order randomOrder(){
            Side side = random_enum<Side, 2>();
            OrdType ordType = weighted_random_enum<OrdType>(
                {
                    1.0, // MARKET
                    1.01, // LIMIT
                    0.0, // STOP
                    0.0, // STOPLIMIT
                }
            );
            long qty = qtyDistrib(gen);
            double price = priceDistrib(gen);
            double stopPriceFactor = stopPriceFactorDistrib(gen);
            double stopPrice;


            switch(side){
                case BUY:
                    price = price - spreadFactor;
                    stopPrice = price + stopPriceFactor;
                    break;
                case SELL:
                    price = price + spreadFactor; 
                    stopPrice = price - stopPriceFactor;
                    break;
            }
            
            return newOrder(side, ordType, qty, price, stopPrice);
        }
};
    
void benchmarkMatcher(){

    InMemoryNotifier notifier;
    Matcher matcher{&notifier};
    OrderFactory ordFactory{};

    int numOrders = 5000000;
    std::vector<Order> orders{};

    for(int i = 0; i <= numOrders; i++){
        orders.push_back(ordFactory.randomOrder());
    }

    std::cout << "Generated orders. Running benchmark..." << std::endl;
    size_t processed = 0;
    auto last_print = std::chrono::steady_clock::now();

    for (auto &order : orders) {
        matcher.addOrder(order);
        ++processed;

        auto now = std::chrono::steady_clock::now();
        if (now - last_print >= std::chrono::seconds(1)) {
            auto counts = matcher.getOrderCounts();
            auto spread = matcher.getSpread();
            std::cout << processed << " orders processed | "
                      << "MARKET:" << counts[MARKET]
                      << " LIMIT:" << counts[LIMIT]
                      << " STOP:" << counts[STOP]
                      << " STOPLIMIT:" << counts[STOPLIMIT]

                      << " | "
                      << "Matches found:" << notifier.matches.size()
                      << " | Spread:";

            if (spread.bidsMissing) {
                std::cout << " bidsMissing";
            } else {
                std::cout << " highestBid:" << spread.highestBid;
            }

            if (spread.asksMissing) {
                std::cout << " asksMissing";
            } else {
                std::cout << " lowestAsk:" << spread.lowestAsk;
            }

            std::cout << "\n";
            last_print = now;
        }
    }
    std::cout << "Done!" << std::endl;
    std::cout << "Matches Found: " << notifier.matches.size() << std::endl;
    std::cout << "Orders Rejected: " << notifier.placementFailedOrders.size() << std::endl;

};

//TODO also show tick stats per second. accumultate and reset every second

void showObservationsAndStats(const Observation& observations, const TickStats& tickStats){
    constexpr int assetWidth = 16;
    constexpr int priceWidth = 10;
    constexpr int volumeWidth = 10;
    constexpr int backlogWidth = 12;

    auto printPrice = [](bool missing, unsigned short price) {
        if (missing) {
            std::cout << std::setw(priceWidth) << "-";
            return;
        }

        std::cout << std::setw(priceWidth) << price;
    };

    std::cout << "\x1B[2J\x1B[H";
    std::cout << "Tick: " << observations.time
              << " | Orders placed: " << tickStats.ordersPlaced
              << " | Orders canceled: " << tickStats.ordersCanceled
              << "\n\n";
    std::cout << std::left
              << std::setw(assetWidth) << "Asset"
              << std::setw(priceWidth) << "Bid"
              << std::setw(priceWidth) << "Ask"
              << std::setw(priceWidth) << "Spread"
              << std::setw(volumeWidth) << "Volume"
              << std::setw(backlogWidth) << "Mkt Bid Qty"
              << std::setw(backlogWidth) << "Mkt Ask Qty"
              << "Market"
              << "\n";
    std::cout << std::string(assetWidth + (priceWidth * 3) + volumeWidth + (backlogWidth * 2) + 6, '-') << "\n";

    if (observations.assetObservations.empty()) {
        std::cout << "No spreads available.\n";
        std::cout.flush();
        return;
    }

    for (const auto& [asset, ao] : observations.assetObservations) {
        const Spread& spread = ao.spread;
        unsigned long volume = ao.volumePerTick;
        //const MarketBacklog& backlog = ao.marketBacklog;

        std::cout << std::left << std::setw(assetWidth) << asset;
        std::cout << std::right;
        printPrice(spread.bidsMissing, spread.highestBid);
        printPrice(spread.asksMissing, spread.lowestAsk);

        if (spread.bidsMissing || spread.asksMissing) {
            std::cout << std::setw(priceWidth) << "-";
        } else {
            std::cout << std::setw(priceWidth)
                      << (spread.lowestAsk - spread.highestBid);
        }

        std::cout << std::setw(volumeWidth) << volume;
        //std::cout << std::setw(backlogWidth) << backlog.bidMarketQty;
        //std::cout << std::setw(backlogWidth) << backlog.askMarketQty;

        std::cout << "  ";
        if (spread.bidsMissing && spread.asksMissing) {
            std::cout << "No bids or asks";
        } else if (spread.bidsMissing) {
            std::cout << "No bids";
        } else if (spread.asksMissing) {
            std::cout << "No asks";
        } else {
            std::cout << "Two-sided";
        }

        std::cout << "\n";
    }

    std::cout.flush();
}

void tinkerWithABM_ConsumptionEconV1(){

    // Setup
    int numSteps = 10000;
    auto abm = std::make_shared<ABM>();

    // TODO - Perhaps some kind of selection could be done to find the optimial population of manufacturers per recipe
    
    /*
    -  use LABOR to simulate population? that might help make this a closed loop
    -  define recipies in a json, then use a loop to automatically create manufacturer populations.
    -  add population limits? this might help impose some kind of darwinina selection. 
        manufacturer agents with the best recipe for the market conditions survive
    
    - f(market_conditions, agent_environment) -> optimal recipe
    
    */

    Recipe refineOil({{"OIL", 24}}, {{"FUEL", 4}, {"FERTILIZER", 1}, {"PLASTIC", 2}}, 10);
    Recipe growFood({{"FERTILIZER", 10}, {"FUEL", 5}}, {{"FOOD", 2}}, 5);
    Recipe smeltSteel({{"COAL", 5}, {"ENERGY", 100}, {"IRON_ORE", 5}}, {{"STEEL", 4}}, 5);
    Recipe coalToEnergy({{"COAL", 20}}, {{"ENERGY", 10}}, 5);
    Recipe makeGoods({{"STEEL", 2}, {"PLASTIC", 10}}, {{"ENERGY", 10}, {"GOODS", 5}});
    Recipe pyrolysis({{"PLASTIC", 10}}, {{"OIL", 1}});

    auto driller = ProducerManager(abm, "Driller", "OIL");
    driller.changeNumAgents(1);
    driller.changePreferedPrice(50, 0);

    auto coalMiner = ProducerManager(abm, "Coal Miner", "COAL");
    coalMiner.changeNumAgents(1);
    coalMiner.changePreferedPrice(10, 0);

    auto ironMiner = ProducerManager(abm, "Iron Miner", "IRON_ORE");
    ironMiner.changeNumAgents(1);
    ironMiner.changePreferedPrice(20, 0);

    auto powerPlant = ManufacturerManager(abm, "Coal Power Plant", coalToEnergy);
    powerPlant.changeNumAgents(5);
    powerPlant.numAgentsFixed = false;

    auto pyrolysisPlant = ManufacturerManager(abm, "Pyrolysis Plant", pyrolysis);
    pyrolysisPlant.changeNumAgents(5);
    pyrolysisPlant.numAgentsFixed = false;

    auto refinery = ManufacturerManager(abm, "Refinery", refineOil);
    refinery.changeNumAgents(20);
    refinery.numAgentsFixed = false;

    auto steelFoundery = ManufacturerManager(abm, "Steel Foundery", smeltSteel);
    steelFoundery.changeNumAgents(20);
    steelFoundery.numAgentsFixed = false;

    auto factory = ManufacturerManager(abm, "Factory", makeGoods);
    factory.changeNumAgents(20);
    factory.numAgentsFixed = false;

    auto farm = ManufacturerManager(abm, "Farm", growFood);
    farm.changeNumAgents(5);
    farm.numAgentsFixed = false;

    std::vector<std::string> assetsToConsume{"FUEL", "FOOD", "GOODS", "ENERGY"};
    std::vector<std::unique_ptr<ConsumerManager>> consumerManagers{};

    for(auto asset : assetsToConsume){
        auto consumers = std::make_unique<ConsumerManager>(
            abm,
            asset + " Consumer",
            asset);
        consumers->changeNumAgents(1000);
        consumers->changeHungerDelay(100, 40);
        consumers->changeMaxPrice(20000, 0);
        consumerManagers.push_back(std::move(consumers));
    }
    
    // Show ABM initial state
    std::cout << "Num Agents: " << abm->getNumAgents() << std::endl;

    // Run
    for(int i = 0; i < numSteps; i++){
        abm->simStep();
        showObservationsAndStats(abm->getLatestObservation(), abm->getTickStats());
    }
};

void tinkerWithABM_ConsumptionEconV2(){
    int numSteps = 10000;
    auto abm = std::make_shared<ABM>();

    // Define recipes
    const std::string recipesJson = R"json([
        {
            "_comment": "Subsistance",
            "inputs":  {"LABOR": 2},
            "outputs": {"ENERGY": 1, "WATER" : 1, "FOOD" : 1},
            "cost": 0
        }
    ])json";
    const std::vector<Recipe> recipes = parseRecipesJson(recipesJson);

    // Define people's desires
    const std::vector<std::string> desiredAssets{
        "WATER", "FOOD", "ENERGY"
    };
    std::vector<Desire> desires;
    for(auto& asset : desiredAssets){
        Desire desire{
            asset, 
            tick(10) // Agent dies after 10 ticks without this asset
        };
        desires.push_back(std::move(desire));
    };

    // Create manufacturer managers for all recipes
    for(auto& recipe : recipes){
        auto population = ManufacturerManager(abm, "Manager", recipe);

        // Set starting population to 10
        population.changeNumAgents(10);
        population.numAgentsFixed = false;
    };

    // Create people manager
    auto peopleManager = PersonManager(abm, "Manager");
    peopleManager.desires = std::move(desires);
    peopleManager.population = 100;
    peopleManager.malthusFactor = 25;
    peopleManager.popGrowthPerTick = 0.05;

    // Run
    for(int i = 0; i < numSteps; i++){
        abm->simStep();
        showObservationsAndStats(abm->getLatestObservation(), abm->getTickStats());
    }  
};