#include "manufacturer.h"
#include <algorithm>
#include <limits>

Manufacturer::Manufacturer(std::shared_ptr<ManufacturerState> state_)
    : Agent(), state(std::move(state_))
{}

std::int64_t Manufacturer::costOfProd(
    const Recipe& recipe,
    const Observation& observation)
{
    std::int64_t totalCost = recipe.cost;

    for (const auto& [asset, qty] : recipe.inputs) {
        auto spreadIt = observation.assetObservations.find(asset);
        std::int32_t bidPrice = 1;

        if (spreadIt != observation.assetObservations.end() && !spreadIt->second.spread.bidsMissing) {
            // Outbid the current highest bid by 1. Attempts to get procurement to fill.
            bidPrice = std::min<std::int32_t>(
                spreadIt->second.spread.highestBid + 1,
                std::numeric_limits<std::int32_t>::max());
        }

        totalCost += static_cast<std::int64_t>(qty) * bidPrice;
    }

    return totalCost;
}

std::int64_t Manufacturer::saleRevenue(
    const Recipe& recipe,
    const Observation& observation)
{
    std::int64_t totalRevenue = 0;

    for (const auto& [asset, qty] : recipe.outputs) {
        auto spreadIt = observation.assetObservations.find(asset);
        std::int32_t salePrice = 0;

        if (spreadIt != observation.assetObservations.end() && !spreadIt->second.spread.bidsMissing) {
            salePrice = spreadIt->second.spread.highestBid;
        }

        totalRevenue += static_cast<std::int64_t>(qty) * salePrice;
    }

    return totalRevenue;
}

std::vector<Order> Manufacturer::procurementOrders(
    const Recipe& recipe,
    const Observation& observation)
{
    std::vector<Order> orders;
    orders.reserve(recipe.inputs.size());

    for (const auto& [asset, requiredQty] : recipe.inputs) {
        std::int64_t deficit = static_cast<std::int64_t>(requiredQty) - state->inventory.assetBalance(asset);
        if (deficit <= 0) {
            continue;
        }

        std::int32_t bidPrice = 1;
        auto spreadIt = observation.assetObservations.find(asset);
        if (spreadIt != observation.assetObservations.end() && !spreadIt->second.spread.bidsMissing) {
            bidPrice = std::min<std::int32_t>(
                spreadIt->second.spread.highestBid + 1,
                std::numeric_limits<std::int32_t>::max());
        }

        orders.push_back(OrderBuilder()
            .limit(BUY, bidPrice, static_cast<std::uint32_t>(std::min<std::int64_t>(
                deficit,
                std::numeric_limits<std::uint32_t>::max())))
            .withAsset(asset)
            .withTraderId(traderId)
            .withIncrementedOrderId()
            .build());
    }

    return orders;
}

void Manufacturer::craft() {
    if (state->recipe.inputs.empty()) {
        return;
    }

    std::int64_t craftCount = std::numeric_limits<std::int64_t>::max();
    bool hasPositiveInput = false;

    for (const auto& [asset, requiredQty] : state->recipe.inputs) {
        if (requiredQty == 0) {
            continue;
        }

        hasPositiveInput = true;
        craftCount = std::min(
            craftCount,
            state->inventory.assetBalance(asset) / static_cast<std::int64_t>(requiredQty));
    }

    if (!hasPositiveInput || craftCount <= 0) {
        return;
    }

    for (const auto& [asset, requiredQty] : state->recipe.inputs) {
        if (requiredQty == 0) continue;
        state->inventory.update(asset,
            -static_cast<std::int32_t>(std::min<std::int64_t>(requiredQty * craftCount, std::numeric_limits<std::int32_t>::max())),
            0, traderId);
    }

    for (const auto& [asset, producedQty] : state->recipe.outputs) {
        if (producedQty == 0) continue;
        state->inventory.update(asset,
            static_cast<std::int32_t>(std::min<std::int64_t>(producedQty * craftCount, std::numeric_limits<std::int32_t>::max())),
            0, traderId);
    }
}

std::vector<Order> Manufacturer::sellOrders() {
    std::vector<Order> orders;
    orders.reserve(state->recipe.outputs.size());

    for (const auto& [asset, producedQty] : state->recipe.outputs) {
        (void)producedQty;
        std::int64_t inventoryQty = state->inventory.assetBalance(asset);
        if (inventoryQty <= 0) {
            continue;
        }

        orders.push_back(OrderBuilder()
            .market(SELL, static_cast<std::uint32_t>(std::min<std::int64_t>(
                inventoryQty,
                std::numeric_limits<std::uint32_t>::max())))
            .withAsset(asset)
            .withTraderId(traderId)
            .withOrdId(-1)
            .build());
    }

    return orders;
}

Action Manufacturer::policy(const Observation& observation) {
    std::int64_t prodCost = costOfProd(state->recipe, observation);
    std::int64_t expectedSaleRevenue = saleRevenue(state->recipe, observation);
    std::vector<Order> orders;

    // Only procure inputs if the trade is profitable
    if (expectedSaleRevenue > prodCost) {
        orders = procurementOrders(state->recipe, observation);
    }

    craft();

    auto productOrders = sellOrders();
    orders.insert(orders.end(), productOrders.begin(), productOrders.end());

    ++state->timeSinceLastSale;

    return actionBuilder
        .withOrders(orders)
        .build();
}

void Manufacturer::orderPlaced(std::int64_t orderId, const tick now) {}

void Manufacturer::matchFound(const Match& match, const tick now) {
    if (match.seller.traderId == traderId) {
        state->timeSinceLastSale = tick(0);
    }
    state->inventory.update(match, traderId);
}

void Manufacturer::orderCanceled(std::int64_t orderId, const tick now) {}

Action Manufacturer::lastWill(const Observation& observation) {
    std::vector<std::int64_t> doomedOrderIds;
    doomedOrderIds.reserve(state->placedOrders.size());

    for (const auto& [asset, orderId] : state->placedOrders) {
        if (orderId > 0) {
            doomedOrderIds.push_back(orderId);
        }
    }

    return actionBuilder
        .withCancellations(doomedOrderIds)
        .build();
}
