#pragma once

#include "agent.h"
#include "abm.h"
#include <random>
#include <vector>
#include <memory>

class AgentManager{
    protected:
        std::vector<long> traderIdsUnderMgmt;
        std::shared_ptr<ABM> abm;

    public:
        std::string name;

        AgentManager(std::shared_ptr<ABM> abm_, std::string name_){
            abm = abm_;
            name = name_;
        }

        /// @brief produces the desired agent
        /// @return 
        virtual std::unique_ptr<Agent> factory() = 0;

        void create(){
            long traderId = abm->addAgent(std::move(factory()));
            traderIdsUnderMgmt.push_back(traderId);
        };

        virtual ~AgentManager(){
            // Clean up agents before destruction
            abm->removeAgents(traderIdsUnderMgmt);
        };
};

class ConsumerManager : public AgentManager{
    std::mt19937 gen;
    std::vector<std::shared_ptr<ConsumerState>> states;
    std::string asset;

    // Underflow issue?
    std::normal_distribution<double> hungerDelayDist;

    // Would log distribution make more sense? 
    // Mark Zuckerberg has a higher max price than the normal person
    std::normal_distribution<double> maxPriceDist;

    void resampleAgentHungerDelay();
    void reampleAgentMaxPrice();

    public:
        ConsumerManager(
            std::shared_ptr<ABM> abm_,
            std::string name_,
            std::string asset_);

        void changeHungerDelay(unsigned long mean, unsigned long std);
        void changeMaxPrice(unsigned short mean, unsigned short std);

        std::unique_ptr<Agent> factory() override;

        void changeNumAgents(unsigned int numAgents);
};

class ProducerManager : public AgentManager{
    std::mt19937 gen;
    std::vector<std::shared_ptr<ProducerState>> states;
    std::string asset;

    std::normal_distribution<double> preferedPriceDist;

    public:
        void reampleAgentPreferedPrice();

        ProducerManager(
            std::shared_ptr<ABM> abm_,
            std::string name_,
            std::string asset_);

        void changePreferedPrice(unsigned short mean, unsigned short std);

        std::unique_ptr<Agent> factory() override;

        void changeNumAgents(unsigned int numAgents);
};

class ManufacturerManager : public AgentManager{
    std::mt19937 gen;
    std::vector<std::shared_ptr<ManufacturerState>> states;
    Recipe recipe;
    TickCallback* tickCallbackRegistration = nullptr;

    public:
        bool numAgentsFixed = true;

        /// @brief Increase/Decrease the number of agents by this proportion
        double numAgentsScaleFactor = 0.05;
        tick neutralAge = tick(5);
        tick staleAge = tick(10);

        ManufacturerManager(
            std::shared_ptr<ABM> abm_,
            std::string name_,
            Recipe recipe
        );

        ~ManufacturerManager() override;
        std::unique_ptr<Agent> factory() override;     
        void changeNumAgents(unsigned int numAgents);

        const std::vector<std::shared_ptr<ManufacturerState>>& getStates() const {
            return states;
        }

        const Recipe& getRecipe() const {
            return recipe;
        }
        
        /// @brief Find the new number of agents. 
        /// @return 
        unsigned int newAgentPopulation();
};

class PersonManager : public AgentManager{
    std::mt19937 gen;
    std::vector<std::shared_ptr<PersonState>> states;
    TickCallback* tickCallbackRegistration = nullptr;

    // TODO add desires and spending power

    public:
        std::vector<Desire> desires;

        // Give all the same static spending power for now.
        unsigned short spendingPower = 100;
        tick lifeSpan = zeroTick();
        unsigned int population = 0;

        // Adding this to slow infinite agent growth
        /// @brief population growth begins decay beyond this point
        unsigned int malthusFactor = 100;

        /// @brief births = population * min(1, malthusFactor / population)
        float popGrowthPerTick = 0.01;

        PersonManager(std::shared_ptr<ABM> abm_, std::string name_);
        ~PersonManager() override;
        std::unique_ptr<Agent> factory() override;
        void birthNewAgents(unsigned int births);
        const std::vector<std::shared_ptr<PersonState>>& getStates() const {
            return states;
        }

        /// @brief Find the new number of agents. 
        /// @return 
        unsigned int numBirths();
};
