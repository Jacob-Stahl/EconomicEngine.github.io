#pragma once

#include "abm.h"
#include "consumer.h"
#include "producer.h"
#include "manufacturer.h"
#include "person.h"

#include <random>
#include <vector>
#include <memory>

class AgentManager {
    protected:
        std::vector<std::int64_t> traderIdsUnderMgmt;
        std::shared_ptr<ABM> abm;

    public:
        std::string name;

        AgentManager(std::shared_ptr<ABM> abm_, std::string name_)
            : abm(std::move(abm_)), name(std::move(name_))
        {}

        /// @brief Produces a new agent instance.
        virtual std::unique_ptr<Agent> factory() = 0;

        void create() {
            std::int64_t traderId = abm->addAgent(factory());
            traderIdsUnderMgmt.push_back(traderId);
        }

        virtual ~AgentManager() {
            abm->removeAgents(traderIdsUnderMgmt);
        }
};

class ConsumerManager : public AgentManager {
    std::mt19937 gen;
    std::vector<std::shared_ptr<ConsumerState>> states;
    std::string asset;

    std::normal_distribution<double> hungerDelayDist;
    std::normal_distribution<double> maxPriceDist;

    void resampleAgentHungerDelay();
    void resampleAgentMaxPrice();

    public:
        ConsumerManager(
            std::shared_ptr<ABM> abm_,
            std::string name_,
            std::string asset_);

        /// @param mean/std in ticks
        void changeHungerDelay(tick mean, tick std);

        /// @brief Prices can be negative. Mean/std are signed.
        void changeMaxPrice(std::int32_t mean, std::int32_t std);

        std::unique_ptr<Agent> factory() override;
        void changeNumAgents(std::uint32_t numAgents);
};

class ProducerManager : public AgentManager {
    std::mt19937 gen;
    std::vector<std::shared_ptr<ProducerState>> states;
    std::string asset;

    std::normal_distribution<double> preferedPriceDist;

    void resampleAgentPreferedPrice();

    public:
        ProducerManager(
            std::shared_ptr<ABM> abm_,
            std::string name_,
            std::string asset_);

        /// @brief Prices can be negative. Mean/std are signed.
        void changePreferedPrice(std::int32_t mean, std::int32_t std);

        std::unique_ptr<Agent> factory() override;
        void changeNumAgents(std::uint32_t numAgents);
};

class ManufacturerManager : public AgentManager {
    std::mt19937 gen;
    std::vector<std::shared_ptr<ManufacturerState>> states;
    Recipe recipe;
    TickCallback* tickCallbackRegistration = nullptr;

    public:
        bool numAgentsFixed = true;

        /// @brief Scale factor applied to (recentSales - stale) to compute agent population delta.
        double numAgentsScaleFactor = 0.05;

        /// @brief Agents initialized to this timeSinceLastSale so new agents aren't immediately culled.
        tick neutralAge = 5;

        /// @brief Agents whose timeSinceLastSale exceeds this are considered stale.
        tick staleAge = 10;

        ManufacturerManager(
            std::shared_ptr<ABM> abm_,
            std::string name_,
            Recipe recipe_);

        ~ManufacturerManager() override;
        std::unique_ptr<Agent> factory() override;
        void changeNumAgents(std::uint32_t numAgents);

        /// @brief Compute the desired population for next tick based on sales activity.
        std::uint32_t newAgentPopulation();

        const std::vector<std::shared_ptr<ManufacturerState>>& getStates() const { return states; }
        const Recipe& getRecipe() const { return recipe; }
};

class PersonManager : public AgentManager {
    std::mt19937 gen;
    std::vector<std::shared_ptr<PersonState>> states;
    TickCallback* tickCallbackRegistration = nullptr;

    public:
        std::vector<Desire> desires;

        std::int32_t spendingPower = 100;
        tick lifeSpan = 0;
        std::uint32_t population = 0;

        /// @brief Population growth begins to decay beyond this size.
        std::uint32_t malthusFactor = 100;

        /// @brief births = population * min(1, malthusFactor / population) * popGrowthPerTick
        float popGrowthPerTick = 0.01f;

        PersonManager(std::shared_ptr<ABM> abm_, std::string name_);
        ~PersonManager() override;

        std::unique_ptr<Agent> factory() override;
        void birthNewAgents(std::uint32_t births);

        /// @brief Compute the number of births for this tick.
        std::uint32_t numBirths();

        const std::vector<std::shared_ptr<PersonState>>& getStates() const { return states; }
};
