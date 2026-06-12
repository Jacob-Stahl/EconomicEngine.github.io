#include "abm.h"
#include "utils.h"

#include <algorithm>

void ABM::observe(){
    latestObservation.time = tickCounter;
    for(auto& it : orderMatchers){
        //auto& ao = latestObservation.assetObservations[it.first];
        //ao.spread = it.second.getSpread();
        //ao.depth = it.second.getDepth();
        //ao.marketBacklog = it.second.getMarketBacklog();
    };
};

void ABM::addMatcherIfNeeded(const std::string& asset){
    if(orderMatchers.find(asset) == orderMatchers.end()){
        orderMatchers.emplace(asset, Matcher(&notifier));
    }
};

void ABM::routeMatches(std::vector<Match>& matches){
    // Agent traderIds MUST be sorted in accending order. 
    // This is critical for this method to work properly.
    // New agent traderIds are always incremented by 1 when they are pushed back to the agents vector. 
    // Old agents are popped without reordering.

    // Sort by buyers.
    std::sort(matches.begin(), matches.end(),
            [](const Match& a, const Match& b)
            {return a.buyer.traderId < b.buyer.traderId; });

    // Route matches to buyers
    size_t agentIdx = 0;
    for(auto& match : matches){
        while(agentIdx < agents.size() && agents[agentIdx]->traderId < match.buyer.traderId){
            ++agentIdx;
        }
        if (agentIdx < agents.size() && agents[agentIdx]->traderId == match.buyer.traderId) {
            agents[agentIdx]->matchFound(match, tickCounter);
        }

        // While we are here, update volume per tick
        updateAssetVolumePerTick(match.buyer.asset, match.qty);
    }

    // Sort by sellers.
    std::sort(matches.begin(), matches.end(),
        [](const Match& a, const Match& b)
        {return a.seller.traderId < b.seller.traderId; });

    // Route matches to sellers
    agentIdx = 0;
    for(auto& match : matches){
        while(agentIdx < agents.size() && agents[agentIdx]->traderId < match.seller.traderId){
            ++agentIdx;
        }
        if (agentIdx < agents.size() && agents[agentIdx]->traderId == match.seller.traderId) {
            agents[agentIdx]->matchFound(match, tickCounter);
        }
    }
    
    matches.clear();
};

void ABM::cancelOrderWithAllMatchers(long doomedOrderId){
    for(auto& it : orderMatchers){
        it.second.cancelOrder(doomedOrderId);
    };
}

void ABM::cleanupCanceledOrdersWithAllMatchers(){
    for(auto& it : orderMatchers){
        it.second.cleanupCanceledOrders();
    };
}

void ABM::simStep(){
    // update latest observation
    tickStats = {};
    clearAssetVolumePerTick();

    // Execute actions for all agents
    for(auto& agent: agents){
        auto action = agent->policy(latestObservation);

        // Handle Cancellations
        for(auto doomedOrderId : action.orderIdsToCancel){
            cancelOrderWithAllMatchers(doomedOrderId);
            agent->orderCanceled(doomedOrderId, tickCounter);
            ++tickStats.ordersCanceled;
        }

        // Handle Order Placements
        for(const auto& requestedOrder : action.ordersToPlace){
            Order order{requestedOrder};
            order.traderId = agent->traderId;
            order.ordId = ++nextOrderId;
            addMatcherIfNeeded(order.asset);

            // TODO: delay matching until all orders are added?
            orderMatchers.at(order.asset).addOrder(order);
            ++tickStats.ordersPlaced;
            
            if(!notifier.placedOrders.empty() && notifier.placedOrders.back().ordId == order.ordId){
                notifier.placedOrders.pop_back();
                agent->orderPlaced(order.ordId, tickCounter);
            }
            else if(!notifier.placementFailedOrders.empty() && notifier.placementFailedOrders.back().ordId == order.ordId)
            {
                notifier.placementFailedOrders.pop_back();
                // TODO: notify placement failed?
            }
        }
    }

    routeMatches(notifier.matches);
    ++tickCounter;

    if(tickCounter.raw() % cleanupCancelledEvery == 0){
        cleanupCanceledOrdersWithAllMatchers();
    }

    // Update latestObservation for callbacks and next simStep() call
    observe();
    runTickCallbacks();
};

long ABM::addAgent(std::unique_ptr<Agent> agent){
    long id = nextTraderId++;
    agent->traderId = id;
    agents.push_back(std::move(agent));
    return id;
}

void ABM::removeAgents(std::vector<long>& traderIdsToRemove){
    std::vector<size_t> agentsToRemove{};
    size_t numAgents = agents.size();
    agentsToRemove.reserve(numAgents);
    std::sort(traderIdsToRemove.begin(), traderIdsToRemove.end(),
        [](const long a, const long b){
            return a < b;
        });

    size_t j = 0;
    for(size_t i = 0; i < numAgents; ++i){
        if(j >= traderIdsToRemove.size()) break;
        auto& agent = agents[i];
        if(agent->traderId == traderIdsToRemove[j]){
            agentsToRemove.push_back(i);
            j++;
        }
    }

    removeAgents(agentsToRemove);
}

void ABM::removeAgents(const std::vector<size_t>& agentsToRemove){
    for(auto agentIdx : agentsToRemove){
        auto& agent = agents[agentIdx];

        // Carry out final will
        auto finalAction = agent->lastWill(latestObservation);
        for(auto doomedOrderId : finalAction.orderIdsToCancel){
            cancelOrderWithAllMatchers(doomedOrderId);
        }

        // TODO: Order placements after death not enforceable yet. fine for now
    }

    // Out to pasture
    removeIdxs<std::unique_ptr<Agent>>(agents, agentsToRemove);
}

void ABM::runTickCallbacks(){
    for(auto& callback : tickCallbacks){
        callback->callBackAction();
    }
}

TickCallback* ABM::addTickCallback(std::unique_ptr<TickCallback> callback){
    TickCallback* rawCallback = callback.get();
    tickCallbacks.push_back(std::move(callback));
    return rawCallback;
}

void ABM::removeTickCallback(TickCallback* callback){
    tickCallbacks.erase(
        std::remove_if(
            tickCallbacks.begin(),
            tickCallbacks.end(),
            [callback](const std::unique_ptr<TickCallback>& candidate) {
                return candidate.get() == callback;
            }),
        tickCallbacks.end());
}

void ABM::updateAssetVolumePerTick(std::string asset, unsigned long change){
    //latestObservation.assetObservations[asset].volumePerTick += change;
}

void ABM::clearAssetVolumePerTick(){
    //for(auto& [asset, ao] : latestObservation.assetObservations){
    //    ao.volumePerTick = 0;
    //}
}