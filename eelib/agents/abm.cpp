#include "abm.h"
#include <algorithm>

void ABM::observe() {
    latestObservation.time = tickCounter;
    for (auto& [asset, matcher] : orderMatchers) {
        auto& ao = latestObservation.assetObservations[asset];
        ao.spread = matcher.getSpread();
        ao.depth = matcher.getDepth();
    }
}

void ABM::addMatcherIfNeeded(const std::string& asset) {
    if (orderMatchers.find(asset) == orderMatchers.end()) {
        orderMatchers.emplace(asset, Matcher{});
    }
}

void ABM::routeMatches() {
    // Collect all pending matches from every matcher's notifier
    std::vector<Match> matches;
    for (auto& [asset, matcher] : orderMatchers) {
        auto& pending = matcher.notifier->matches;
        matches.insert(matches.end(), pending.begin(), pending.end());
        pending.clear();
    }

    if (matches.empty()) return;

    // Agent traderIds are kept in ascending order (new agents always appended).

    // Route to buyers
    std::sort(matches.begin(), matches.end(),
        [](const Match& a, const Match& b) { return a.buyer.traderId < b.buyer.traderId; });

    size_t agentIdx = 0;
    for (const auto& match : matches) {
        while (agentIdx < agents.size() && agents[agentIdx]->traderId < match.buyer.traderId)
            ++agentIdx;
        if (agentIdx < agents.size() && agents[agentIdx]->traderId == match.buyer.traderId)
            agents[agentIdx]->matchFound(match, tickCounter);

        updateAssetVolumePerTick(match.buyer.asset, match.qty);
    }

    // Route to sellers
    std::sort(matches.begin(), matches.end(),
        [](const Match& a, const Match& b) { return a.seller.traderId < b.seller.traderId; });

    agentIdx = 0;
    for (const auto& match : matches) {
        while (agentIdx < agents.size() && agents[agentIdx]->traderId < match.seller.traderId)
            ++agentIdx;
        if (agentIdx < agents.size() && agents[agentIdx]->traderId == match.seller.traderId)
            agents[agentIdx]->matchFound(match, tickCounter);
    }
}

void ABM::cancelOrderWithAllMatchers(std::int64_t doomedOrderId) {
    for (auto& [asset, matcher] : orderMatchers) {
        matcher.cancelOrder(doomedOrderId);
    }
}

void ABM::simStep() {
    tickStats = {};
    clearAssetVolumePerTick();

    for (auto& agent : agents) {
        auto action = agent->policy(latestObservation);

        // Handle cancellations
        for (auto doomedOrderId : action.orderIdsToCancel) {
            cancelOrderWithAllMatchers(doomedOrderId);
            agent->orderCanceled(doomedOrderId, tickCounter);
            ++tickStats.ordersCanceled;
        }

        // Handle order placements
        for (const auto& requestedOrder : action.ordersToPlace) {
            Order order{requestedOrder};
            addMatcherIfNeeded(order.asset);

            // TODO: delay matching until all orders are placed?
            orderMatchers.at(order.asset).placeOrder(order);
            ++tickStats.ordersPlaced;

            // TODO: check placement result via notifier once placedOrders/placementFailed
            // are tracked in Notifier. For now, notify unconditionally.
            agent->orderPlaced(order.ordId, tickCounter);
        }
    }

    routeMatches();
    ++tickCounter;

    observe();
    runTickCallbacks();
}

void ABM::removeAgents(std::vector<std::int64_t>& traderIdsToRemove) {
    std::vector<size_t> agentsToRemove;
    size_t numAgents = agents.size();
    agentsToRemove.reserve(numAgents);

    // if traderIdsToRemove comes in sorted, we can remove this sort
    std::sort(traderIdsToRemove.begin(), traderIdsToRemove.end());

    size_t j = 0;
    for (size_t i = 0; i < numAgents; ++i) {
        if (j >= traderIdsToRemove.size()) break;
        if (agents[i]->traderId == traderIdsToRemove[j]) {
            agentsToRemove.push_back(i);
            ++j;
        }
    }

    removeAgents(agentsToRemove);
}

void ABM::removeAgents(const std::vector<size_t>& agentsToRemove) {
    for (auto agentIdx : agentsToRemove) {
        auto& agent = agents[agentIdx];

        // Carry out final will
        auto finalAction = agent->lastWill(latestObservation);
        for (auto doomedOrderId : finalAction.orderIdsToCancel) {
            cancelOrderWithAllMatchers(doomedOrderId);
        }

        // TODO: order placements in lastWill not yet supported
    }

    // Erase removed agents, preserving relative order
    size_t write = 0;
    size_t removeIdx = 0;
    for (size_t i = 0; i < agents.size(); ++i) {
        bool removed = removeIdx < agentsToRemove.size() && agentsToRemove[removeIdx] == i;
        if (removed) {
            ++removeIdx;
        } else {
            if (write != i) agents[write] = std::move(agents[i]);
            ++write;
        }
    }
    agents.resize(write);
}

void ABM::runTickCallbacks() {
    for (auto& callback : tickCallbacks) {
        callback->callBackAction();
    }
}

TickCallback* ABM::addTickCallback(std::unique_ptr<TickCallback> callback) {
    TickCallback* rawPtr = callback.get();
    tickCallbacks.push_back(std::move(callback));
    return rawPtr;
}

void ABM::removeTickCallback(TickCallback* callback) {
    tickCallbacks.erase(
        std::remove_if(tickCallbacks.begin(), tickCallbacks.end(),
            [callback](const std::unique_ptr<TickCallback>& c) {
                return c.get() == callback;
            }),
        tickCallbacks.end());
}

void ABM::updateAssetVolumePerTick(const std::string& asset, std::uint32_t qty) {
    latestObservation.assetObservations[asset].volumePerTick += qty;
}

void ABM::clearAssetVolumePerTick() {
    for (auto& [asset, ao] : latestObservation.assetObservations) {
        ao.volumePerTick = 0;
    }
}
