#pragma once

#include "../matcher.h"
#include "../match.h"
#include "../tick.h"

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
    std::int64_t volumePerTick = 0;
};

// TODO: create AssetObservation struct with all properties, and use a single asset - AssetObservation map
struct Observation{
    tick time;
    std::map<std::string, AssetObservation> assetObservations;
};

struct Action{
    std::vector<Order> ordersToPlace;
    std::vector<std::int64_t> orderIdsToCancel;

    private:
        Action() = default;

    friend class ActionBuilder;
};

class ActionBuilder{
    Action action{};

    public:
        ActionBuilder& withOrder(const Order& order){
            action.ordersToPlace.push_back(order);
            return *this;
        }
        ActionBuilder& withOrders(std::vector<Order>& orders){
            action.ordersToPlace.insert(action.ordersToPlace.end(), orders.begin(), orders.end());
            return *this;
        }
        ActionBuilder& withCancellation(std::int64_t ordId){
            action.orderIdsToCancel.push_back(ordId);
            return *this;
        }
        ActionBuilder& withCancellations(const std::vector<std::int64_t>& ordIds){
            action.orderIdsToCancel.insert(action.orderIdsToCancel.end(), ordIds.begin(), ordIds.end());
            return *this;
        }

        Action build(){
            auto builtAction = std::move(action);
            action = Action{};
            return builtAction;
        }
};

class Agent{
    public:
        std::int64_t traderId;
        ActionBuilder actionBuilder;

        Agent(std::int64_t id) : traderId(id) {};
        virtual ~Agent() = default;

        virtual Action policy(const Observation& observation){return actionBuilder.build();};;

        virtual void matchFound(const Match& match, const tick now){};
        virtual void orderPlaced(std::int64_t orderId, const tick now){};
        virtual void orderCanceled(std::int64_t orderId, const tick now){};

        /// @brief Final action before agent is removed from ABM
        virtual Action lastWill(const Observation& observation){return actionBuilder.build();};
};

struct Recipe {
    // Asset - Amount
    std::map<std::string, std::uint32_t> inputs;
    std::map<std::string, std::uint32_t> outputs;

    std::int32_t cost = 0;

    Recipe() = default;

    Recipe(
        std::initializer_list<std::pair<const std::string, std::uint32_t>> inputs_,
        std::initializer_list<std::pair<const std::string, std::uint32_t>> outputs_,
        std::int32_t cost_ = 0)
        : inputs(inputs_),
          outputs(outputs_),
          cost(cost_)
    {}
};