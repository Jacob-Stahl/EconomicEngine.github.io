#pragma once

#include "matcher.h"
#include "match.h"
#include "tick.h"

#include <string>
#include <functional>
#include <map>
#include <initializer_list>
#include <memory>
#include <vector>


struct AssetObservation{
    Spread spread;
    Depth depth;
    //MarketBacklog marketBacklog;
    long volumePerTick = 0;
};

// TODO: create AssetObservation struct with all properties, and use a single asset - AssetObservation map
struct Observation{
    tick time;
    std::map<std::string, AssetObservation> assetObservations;
};

struct Action{
    std::vector<Order> ordersToPlace;
    std::vector<long> orderIdsToCancel;

    Action() = default;

    explicit Action(const Order& order_){
        ordersToPlace.push_back(order_);
    }

    Action(const Order& order_, long doomedOrderId_){
        ordersToPlace.push_back(order_);
        orderIdsToCancel.push_back(doomedOrderId_);
    }

    explicit Action(long doomedOrderId_){
        orderIdsToCancel.push_back(doomedOrderId_);
    }

    explicit Action(std::vector<Order> ordersToPlace_)
        : ordersToPlace(std::move(ordersToPlace_))
    {}

    explicit Action(std::vector<long> orderIdsToCancel_)
        : orderIdsToCancel(std::move(orderIdsToCancel_))
    {}

    Action(std::vector<Order> ordersToPlace_, std::vector<long> orderIdsToCancel_)
        : ordersToPlace(std::move(ordersToPlace_)),
          orderIdsToCancel(std::move(orderIdsToCancel_))
    {}

    void addOrder(const Order& order_){
        ordersToPlace.push_back(order_);
    }

    void addCancellation(long doomedOrderId_){
        orderIdsToCancel.push_back(doomedOrderId_);
    }

    bool empty() const {
        return ordersToPlace.empty() && orderIdsToCancel.empty();
    }
};

class Agent{
    public:
        long traderId;
        Agent(long);
        virtual ~Agent() = default;

        virtual Action policy(const Observation& observation);

        virtual void matchFound(const Match& match, const tick now){};
        virtual void orderPlaced(long orderId, const tick now){};
        virtual void orderCanceled(long orderId, const tick now){};

        /// @brief Final action before agent is removed from ABM
        virtual Action lastWill(const Observation& observation){return Action();};
};


struct ConsumerState{
    std::string asset;
    tick sinceLastFill = tick(0);
    long orderOnBookId = 0;
    unsigned short maxPrice;
    tick hungerDelay = tick(0);
};

class Consumer : public Agent{

    private:
        std::shared_ptr<ConsumerState> state;

    public:
        Consumer(long traderId_, std::shared_ptr<ConsumerState> state_);
        Consumer(long traderId_, std::string asset_, unsigned short maxPrice_,
            tick hungerDelay_);
        Action policy(const Observation& observation) override;
        void orderPlaced(long orderId, const tick now) override;
        void matchFound(const Match& match, const tick now) override;
        void orderCanceled(long orderId, const tick now) override;
        Action lastWill(const Observation& observation) override;
};

struct ProducerState {
    std::string asset;
    unsigned short preferedPrice;
    unsigned int qtyPerTick = 1;
};

class Producer : public Agent{
    private:
        std::shared_ptr<ProducerState> state;

    public:
        Producer(long traderId_, std::shared_ptr<ProducerState> state_);
        Producer(long traderId_, std::string asset, 
            unsigned short preferedPrice);
        Action policy(const Observation& observation) override;
};

inline double fast_sigmoid(double x) {
    return x / (1 + std::abs(x));
};

struct Recipe {
    // Asset - Amount
    std::map<std::string, int> inputs;
    std::map<std::string, int> outputs;

    // Could be negative, if the recipe produces money...
    long cost = 0;

    Recipe() = default;

    Recipe(
        std::initializer_list<std::pair<const std::string, int>> inputs_,
        std::initializer_list<std::pair<const std::string, int>> outputs_,
        long cost_ = 0)
        : inputs(inputs_),
          outputs(outputs_),
          cost(cost_)
    {}
};

std::vector<Recipe> parseRecipesJson(const std::string& jsonText);

class Inventory {
    std::map<std::string, long> assets{};
    long cashBalance = 0;

    public:
        Inventory() = default;
        void update(const Match& match, long thisTraderId);
        void update(
            const std::string& asset,
            int qtyChange,
            long cashChange,
            long thisTraderId);

        int qty(const std::string& asset) const {
            auto it = assets.find(asset);
            return it == assets.end() ? 0 : it->second;
        }

        long cash() const {
            return cashBalance;
        }
};

struct ManufacturerState {
    Recipe recipe;
    Inventory inventory{};
    tick timeSinceLastSale = tick(0);

    // Asset - OrderId
    std::map<std::string, long> placedOrders;
};

class Manufacturer : public Agent{
    private:
        std::shared_ptr<ManufacturerState> state;
        long costOfProd(const Recipe& recipe, const Observation& observation);
        long saleRevenue(const Recipe& recipe, const Observation& observation);

        /// @brief Place orders required to fill remaining prerequisites for the recipe.
        /// @param recipe 
        /// @param observation 
        /// @return 
        std::vector<Order> procurementOrders(const Recipe& recipe, const Observation& observation);

        /// @brief Craft as many products as possible with the availible ingredients
        void craft();

        /// @brief Sell all products in inventory
        /// @return 
        std::vector<Order> sellOrders();
    public:
        Manufacturer(long traderId_, std::shared_ptr<ManufacturerState> state);
        Action policy(const Observation& observation) override;
        void orderPlaced(long orderId, const tick now) override;
        void matchFound(const Match& match, const tick now) override;
        void orderCanceled(long orderId, const tick now) override;
        Action lastWill(const Observation& observation) override;
};


struct Desire{
    std::string asset;
    tick deathTheshhold = tick(0);

    tick ticksSinceLastConsumption = tick(0);

    float proportionToDeath() const;
};

std::vector<Desire> parseDesiresJson(const std::string& jsonText);

struct PersonState{
    std::vector<Desire> desires;

    /// @brief the highest price an agent is able to bid for a desire
    unsigned short spendingPower = 0;
    tick lifeSpan = tick(0);

    tick age = tick(0);
    long lastPlacedBuyId = -1;

    bool shouldDie() const;
    void incrementAllDesireTicks();
};

class Person : public Agent{
    std::shared_ptr<PersonState> state;

    public:
        Person(
            long traderId_, 
            std::shared_ptr<PersonState> state_
        ) : Agent(traderId_){
            state = state_;
        }

        Action policy(const Observation& observation) override;
        void orderPlaced(long orderId, const tick now) override;
        void matchFound(const Match& match, const tick now) override;
        void orderCanceled(long orderId, const tick now) override;
        Action lastWill(const Observation& observation) override;

    /*
    
    Hungry for multiple assets.
    // Basic needs. Steep hunger curve. Agent dies if it goes a certain amount of ticks without consuming all of these
        - WATER
        - PROTIEN
        - CARBS
        - SUGAR

    // ?? With limited money, how to agents prioritize what to purchase? what they are most hungry for at this instant? what they run out of first?

    // Maslows Hierarchy of needs. https://en.wikipedia.org/wiki/Maslow's_hierarchy_of_needs#/media/File:Maslow's_Hierarchy_of_Needs_Pyramid_(original_five-level_model).png
    // ?? I suppose "assets" higher up on the pyramid could have flatter hunger curves.  Do agents die without esteem? can ESTEEM be a modeled as a commodity?

    */
};