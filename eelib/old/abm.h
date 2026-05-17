# pragma once

#include <memory>
#include <unordered_set>
#include <vector>
#include "matcher.h"
#include "agent.h"

class TickCallback{
    public:
        virtual ~TickCallback() = default;

        /// @brief Called after the end of every tick
        virtual void callBackAction() = 0;
};

struct TickStats{
    unsigned int ordersPlaced = 0;
    unsigned int ordersCanceled = 0;
};

// TODO give markets and stops a TTL. They clog up the book and slow things down.
// Make sure cancelled TTL orders restore the agents inventory

/// @brief Agent Based Model. Framework for multi agent trading simulations.
class ABM{

    TickStats tickStats;

    // TODO: Replace a count of how many cancelled orders are on the books
    // if its over N, run clean up
    const int cleanupCancelledEvery = 1;

    // Agents
    std::vector<std::unique_ptr<Agent>> agents;
    tick tickCounter{0};
    long nextTraderId = 1;
    long nextOrderId = 1;

    /// @brief Asset - Matcher
    std::unordered_map<std::string, Matcher> orderMatchers;
    InMemoryNotifier notifier{};

    Observation latestObservation;

    void cancelOrderWithAllMatchers(long doomedOrderId);
    void cleanupCanceledOrdersWithAllMatchers();
    void addMatcherIfNeeded(const std::string& asset);
    void routeMatches(std::vector<Match>& matches);
    void observe();
    void removeAgents(const std::vector<size_t>& agentsToRemove);
    void runTickCallbacks();
    void clearAssetVolumePerTick();
    void updateAssetVolumePerTick(std::string asset, unsigned long change);

    public:
        TickCallback* addTickCallback(std::unique_ptr<TickCallback> callback);
        void removeTickCallback(TickCallback* callback);

        ABM() = default;
        void simStep();
        long addAgent(std::unique_ptr<Agent> newAgent);
        void removeAgents(std::vector<long>& traderIdsToRemove);
        
        size_t getNumAgents() const { return agents.size(); }
        const Observation& getLatestObservation() {return latestObservation; };
        const TickStats& getTickStats() {return tickStats; };

        std::vector<long> getAgentIds() const {
            std::vector<long> ids;
            ids.reserve(agents.size());
            for (const auto& a : agents) ids.push_back(a->traderId);
            return ids;
        }

    private:
        std::vector<std::unique_ptr<TickCallback>> tickCallbacks{};
};