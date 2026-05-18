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
};

class ActionBuilder{
    Action action{};

    public:
        ActionBuilder& withOrder(const Order& order){
            action.ordersToPlace.push_back(order);
        }
        ActionBuilder& withOrders(std::vector<Order>& orders){
            action.ordersToPlace.insert(action.ordersToPlace.end(), orders.begin(), orders.end());
        }
        ActionBuilder& withCancellation(long ordId){
            action.orderIdsToCancel.push_back(ordId);
        }
        ActionBuilder& withCancellations(const std::vector<long>& ordIds){
            action.orderIdsToCancel.insert(action.orderIdsToCancel.end(), ordIds.begin(), ordIds.end());
        }

        void newAction(){
            action = Action();
        }
        Action build(){
            return action;
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

struct Recipe {
    // Asset - Amount
    std::map<std::string, int> inputs;
    std::map<std::string, int> outputs;

    int cost = 0;

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
