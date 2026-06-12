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
        ActionBuilder& withOrder(const Order& order);
        ActionBuilder& withOrders(std::vector<Order>& orders);
        ActionBuilder& withCancellation(std::int64_t ordId);
        ActionBuilder& withCancellations(const std::vector<std::int64_t>& ordIds);
        Action build();
};

class Agent{
    public:
        std::int64_t traderId = -1;
        ActionBuilder actionBuilder;
        OrderBuilder orderBuilder;

        Agent();
        virtual ~Agent() = default;

        virtual Action policy(const Observation& observation);

        virtual void matchFound(const Match& match, const tick now);
        virtual void orderPlaced(std::int64_t orderId, const tick now);
        virtual void orderCanceled(std::int64_t orderId, const tick now);

        /// @brief Final action before agent is removed from ABM
        virtual Action lastWill(const Observation& observation);
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

class Inventory {
    std::map<std::string, std::int64_t> _assetBalance{};
    std::int64_t _cashBalance = 0;

    public:
        Inventory() = default;
        void update(const Match& match, std::int64_t thisTraderId);
        void update(
            const std::string& asset,
            std::int32_t qtyChange,
            std::int64_t cashChange,
            std::int64_t thisTraderId);

        std::int64_t assetBalance(const std::string& asset) const;
        std::int64_t cashBalance() const;
};