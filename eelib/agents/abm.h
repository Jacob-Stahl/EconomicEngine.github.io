#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <type_traits>

#include "../matcher.h"
#include "agent.h"

class TickCallback {
    public:
        virtual ~TickCallback() = default;

        /// @brief Called after the end of every tick
        virtual void callBackAction() = 0;
};

struct TickStats {
    std::uint32_t ordersPlaced = 0;
    std::uint32_t ordersCanceled = 0;
};

// TODO give markets and stops a TTL. They clog up the book and slow things down.
// Make sure cancelled TTL orders restore the agents inventory

/// @brief Agent Based Model. Framework for multi agent trading simulations.
class ABM {

    TickStats tickStats;

    // Agents
    std::vector<std::unique_ptr<Agent>> agents;
    tick tickCounter = 0;
    std::int64_t nextTraderId = 1;
    std::int64_t nextOrderId = 1;

    /// @brief Asset -> Matcher
    std::unordered_map<std::string, Matcher> orderMatchers;

    Observation latestObservation;

    void cancelOrderWithAllMatchers(std::int64_t doomedOrderId);
    void addMatcherIfNeeded(const std::string& asset);

    /// @brief Collect matches from all matchers and dispatch to agents.
    void routeMatches();

    void observe();
    void removeAgents(const std::vector<size_t>& agentsToRemove);
    void runTickCallbacks();
    void clearAssetVolumePerTick();
    void updateAssetVolumePerTick(const std::string& asset, std::uint32_t qty);

    std::vector<std::unique_ptr<TickCallback>> tickCallbacks{};

    public:
        ABM() = default;

        void simStep();

        template <std::derived_from<Agent> T, typename... Args>
            requires std::constructible_from<T, Args...>
        const T& addAgent(Args&&... args) {
            auto agent = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *agent;
            agents.push_back(std::move(agent));
            return ref;
        }

        void removeAgents(std::vector<std::int64_t>& traderIdsToRemove);

        TickCallback* addTickCallback(std::unique_ptr<TickCallback> callback);
        void removeTickCallback(TickCallback* callback);

        size_t getNumAgents() const { return agents.size(); }
        const Observation& getLatestObservation() const { return latestObservation; }
        const TickStats& getTickStats() const { return tickStats; }

        std::vector<std::int64_t> getAgentIds() const {
            std::vector<std::int64_t> ids;
            ids.reserve(agents.size());
            for (const auto& a : agents) ids.push_back(a->traderId);
            return ids;
        }
};
