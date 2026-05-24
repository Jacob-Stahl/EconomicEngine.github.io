#include "agent.h"

// ActionBuilder

ActionBuilder& ActionBuilder::withOrder(const Order& order) {
    action.ordersToPlace.push_back(order);
    return *this;
}

ActionBuilder& ActionBuilder::withOrders(std::vector<Order>& orders) {
    action.ordersToPlace.insert(action.ordersToPlace.end(), orders.begin(), orders.end());
    return *this;
}

ActionBuilder& ActionBuilder::withCancellation(std::int64_t ordId) {
    action.orderIdsToCancel.push_back(ordId);
    return *this;
}

ActionBuilder& ActionBuilder::withCancellations(const std::vector<std::int64_t>& ordIds) {
    action.orderIdsToCancel.insert(action.orderIdsToCancel.end(), ordIds.begin(), ordIds.end());
    return *this;
}

Action ActionBuilder::build() {
    auto builtAction = std::move(action);
    action = Action{};
    return builtAction;
}

// Agent
Agent::Agent(){}

Action Agent::policy(const Observation& observation) {
    return actionBuilder.build();
}

void Agent::matchFound(const Match& match, const tick now) {}
void Agent::orderPlaced(std::int64_t orderId, const tick now) {}
void Agent::orderCanceled(std::int64_t orderId, const tick now) {}

Action Agent::lastWill(const Observation& observation) {
    return actionBuilder.build();
}

// Inventory

void Inventory::update(const Match& match, std::int64_t thisTraderId) {
    if (match.buyer.traderId == thisTraderId) {
        update(match.buyer.asset,
            static_cast<std::int32_t>(match.qty),
            static_cast<std::int64_t>(match.buyer.price) * match.qty,
            thisTraderId);
    }
    if (match.seller.traderId == thisTraderId) {
        update(match.seller.asset,
            -static_cast<std::int32_t>(match.qty),
            -static_cast<std::int64_t>(match.seller.price) * match.qty,
            thisTraderId);
    }
}

void Inventory::update(
    const std::string& asset,
    std::int32_t qtyChange,
    std::int64_t cashChange,
    std::int64_t thisTraderId)
{
    (void)thisTraderId;
    _assetBalance[asset] += qtyChange;
    _cashBalance += cashChange;
}

std::int64_t Inventory::assetBalance(const std::string& asset) const {
    auto it = _assetBalance.find(asset);
    return it == _assetBalance.end() ? 0 : it->second;
}

std::int64_t Inventory::cashBalance() const {
    return _cashBalance;
}