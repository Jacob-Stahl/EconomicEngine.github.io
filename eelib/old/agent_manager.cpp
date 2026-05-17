#include "agent_manager.h"
#include "agent.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace {

tick clampTickSample(double value) {
    return tick(static_cast<tick::rep>(std::clamp(
        value,
        0.0,
        static_cast<double>(std::numeric_limits<tick::rep>::max()))));
}

unsigned short clampUnsignedShortSample(double value) {
    return static_cast<unsigned short>(std::clamp(
        value,
        0.0,
        static_cast<double>(std::numeric_limits<unsigned short>::max())));
}

}


std::unique_ptr<Agent> AgentManager::factory(){
    return nullptr;
}

// Consumer Manager
ConsumerManager::ConsumerManager(
    std::shared_ptr<ABM> abm_,
    std::string name_,
    std::string asset_): AgentManager(abm_, name_){
        asset = asset_;
        hungerDelayDist = std::normal_distribution<double>(0, 0);
        maxPriceDist = std::normal_distribution<double>(0, 0);
    };

void ConsumerManager::changeHungerDelay(unsigned long mean, unsigned long std){
    hungerDelayDist = std::normal_distribution<double>(mean, std);
    resampleAgentHungerDelay();
};

void ConsumerManager::changeMaxPrice(unsigned short mean, unsigned short std){
    maxPriceDist = std::normal_distribution<double>(mean, std);
    reampleAgentMaxPrice();
};

void ConsumerManager::resampleAgentHungerDelay(){
    for(auto state : states){
        state->hungerDelay = clampTickSample(hungerDelayDist(gen));
    }
};

void ConsumerManager::reampleAgentMaxPrice(){
    for(auto state : states){
        state->maxPrice = clampUnsignedShortSample(maxPriceDist(gen));
    }
};

std::unique_ptr<Agent> ConsumerManager::factory(){
    auto state = std::make_shared<ConsumerState>();
    state->hungerDelay = clampTickSample(hungerDelayDist(gen));
    state->maxPrice = clampUnsignedShortSample(maxPriceDist(gen));
    state->asset = asset;
    states.push_back(state);

    return std::make_unique<Consumer>(0, state);
};


void ConsumerManager::changeNumAgents(unsigned int numAgents){
    long diff = (long)numAgents - (long)states.size();
    std::vector<long> doomedIds{};

    // Create new agents
    while(diff > 0){ 
        create();
        --diff;
    }

    // Destroy Agents
    while(diff < 0){ 
        long doomedId = traderIdsUnderMgmt.back();
        doomedIds.push_back(doomedId);

        traderIdsUnderMgmt.pop_back();
        states.pop_back();
        ++diff;
    }

    if (doomedIds.size() > 0)
        abm->removeAgents(doomedIds);       
}

// Producer Manager
ProducerManager::ProducerManager(
    std::shared_ptr<ABM> abm_,
    std::string name_,
    std::string asset_): AgentManager(abm_, name_){
        asset = asset_;
        preferedPriceDist = std::normal_distribution<double>(0, 0);
    };

void ProducerManager::changePreferedPrice(unsigned short mean, unsigned short std){
    preferedPriceDist = std::normal_distribution<double>(mean, std);
    reampleAgentPreferedPrice();
};

void ProducerManager::reampleAgentPreferedPrice(){
    for(auto state : states){
        state->preferedPrice = clampUnsignedShortSample(preferedPriceDist(gen));
    }
};

std::unique_ptr<Agent> ProducerManager::factory(){
    auto state = std::make_shared<ProducerState>();
    state->preferedPrice = clampUnsignedShortSample(preferedPriceDist(gen));
    state->asset = asset;
    states.push_back(state);

    return std::make_unique<Producer>(0, state);
};

void ProducerManager::changeNumAgents(unsigned int numAgents){
    long diff = static_cast<long>(numAgents) - static_cast<long>(states.size());
    std::vector<long> doomedIds{};

    while(diff > 0){
        create();
        --diff;
    }

    while(diff < 0){
        long doomedId = traderIdsUnderMgmt.back();
        doomedIds.push_back(doomedId);

        traderIdsUnderMgmt.pop_back();
        states.pop_back();
        ++diff;
    }

    if (!doomedIds.empty()) {
        abm->removeAgents(doomedIds);
    }
}

// Manufacturer Manager

class ManufacturerManagerTickCallback : public TickCallback {
    ManufacturerManager* manager;

public:
    explicit ManufacturerManagerTickCallback(ManufacturerManager* manager_)
        : manager(manager_)
    {}

    void callBackAction() override {
        if (manager == nullptr || manager->numAgentsFixed) {
            return;
        }

        manager->changeNumAgents(manager->newAgentPopulation());
    }
};

ManufacturerManager::ManufacturerManager(
    std::shared_ptr<ABM> abm_,
    std::string name_,
    Recipe recipe_)
    : AgentManager(abm_, std::move(name_)),
      recipe(std::move(recipe_))
{
    tickCallbackRegistration = abm->addTickCallback(
        std::make_unique<ManufacturerManagerTickCallback>(this));
}

ManufacturerManager::~ManufacturerManager() {
    if (tickCallbackRegistration != nullptr) {
        abm->removeTickCallback(tickCallbackRegistration);
        tickCallbackRegistration = nullptr;
    }
}

std::unique_ptr<Agent> ManufacturerManager::factory(){
    auto state = std::make_shared<ManufacturerState>(ManufacturerState{
        recipe,
        Inventory(),
        neutralAge, // initialize agents to a neutral age
        {}
    });

    states.push_back(state);
    return std::make_unique<Manufacturer>(0, state);
}

void ManufacturerManager::changeNumAgents(unsigned int numAgents){
    long diff = static_cast<long>(numAgents) - static_cast<long>(states.size());
    std::vector<long> doomedIds{};

    while(diff > 0){
        create();
        --diff;
    }

    while(diff < 0){
        long doomedId = traderIdsUnderMgmt.back();
        doomedIds.push_back(doomedId);

        traderIdsUnderMgmt.pop_back();
        states.pop_back();
        ++diff;
    }

    if (!doomedIds.empty()) {
        abm->removeAgents(doomedIds);
    }
}

unsigned int ManufacturerManager::newAgentPopulation() {
    const long currentCount = static_cast<long>(states.size());

    if (numAgentsFixed || currentCount <= 0) {
        return static_cast<unsigned int>(std::max(0L, currentCount));
    }

    long recentSaleCount = 0;
    long staleCount = 0;

    for (const auto& state : states) {
        if (state->timeSinceLastSale < neutralAge) {
            ++recentSaleCount;
        } else if (state->timeSinceLastSale >= staleAge) {
            ++staleCount;
        }
    }

    const double rawAgentDiff =
        static_cast<double>(recentSaleCount - staleCount) * numAgentsScaleFactor;

    long agentDiff = static_cast<long>(std::round(rawAgentDiff));
    if (agentDiff == 0 && recentSaleCount != staleCount) {
        // Prevents the count from getting stuck with low agent populations
        agentDiff = (recentSaleCount > staleCount) ? 1 : -1;
    }

    // Set the new desired population, with a min of 1
    const long nextCount = std::max(1L, currentCount + agentDiff);

    return static_cast<unsigned int>(nextCount);
}

// Person Manager

// TODO don't forget to kill old or starving agents!
unsigned int PersonManager::numBirths(){
    float growthDecay = std::min(1.0f, malthusFactor / (float)population);
    float growthProportion = popGrowthPerTick * growthDecay; 
    return (population * growthProportion);
}

void PersonManager::birthNewAgents(unsigned int births){
    for(int i = 0; i < births; i++){
        create();
    }
}

std::unique_ptr<Agent> PersonManager::factory(){
    auto state = std::make_shared<PersonState>(PersonState{
        desires,
        spendingPower,
        lifeSpan
    });

    states.push_back(state);
    return std::make_unique<Person>(0, state);
}

class PersonManagerTickCallback : public TickCallback{
    PersonManager* manager;

    public:
        explicit PersonManagerTickCallback(PersonManager* manager_)
            : manager{manager_}
        {}

        void callBackAction() override {
            manager->birthNewAgents(manager->numBirths());
        }
};

PersonManager::PersonManager(
    std::shared_ptr<ABM> abm_,
    std::string name_
) : AgentManager(abm_, std::move(name_))
{
    tickCallbackRegistration = abm->addTickCallback(
        std::make_unique<PersonManagerTickCallback>(this));
};

PersonManager::~PersonManager(){
    if (tickCallbackRegistration != nullptr) {
        abm->removeTickCallback(tickCallbackRegistration);
        tickCallbackRegistration = nullptr;
    }
};