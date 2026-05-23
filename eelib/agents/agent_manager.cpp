#include "agent_manager.h"
#include <cmath>
#include <algorithm>
#include <limits>

// Clamp helpers

static tick clampTickSample(double value) {
    return static_cast<tick>(std::clamp(
        value,
        0.0,
        static_cast<double>(std::numeric_limits<tick>::max())));
}

static std::int32_t clampInt32Sample(double value) {
    return static_cast<std::int32_t>(std::clamp(
        value,
        static_cast<double>(std::numeric_limits<std::int32_t>::min()),
        static_cast<double>(std::numeric_limits<std::int32_t>::max())));
}

// ConsumerManager

ConsumerManager::ConsumerManager(
    std::shared_ptr<ABM> abm_,
    std::string name_,
    std::string asset_)
    : AgentManager(std::move(abm_), std::move(name_)),
      asset(std::move(asset_)),
      hungerDelayDist(0, 0),
      maxPriceDist(0, 0)
{}

void ConsumerManager::changeHungerDelay(tick mean, tick std) {
    hungerDelayDist = std::normal_distribution<double>(
        static_cast<double>(mean), static_cast<double>(std));
    resampleAgentHungerDelay();
}

void ConsumerManager::changeMaxPrice(std::int32_t mean, std::int32_t std) {
    maxPriceDist = std::normal_distribution<double>(
        static_cast<double>(mean), static_cast<double>(std));
    resampleAgentMaxPrice();
}

void ConsumerManager::resampleAgentHungerDelay() {
    for (auto& state : states) {
        state->hungerDelay = clampTickSample(hungerDelayDist(gen));
    }
}

void ConsumerManager::resampleAgentMaxPrice() {
    for (auto& state : states) {
        state->maxPrice = clampInt32Sample(maxPriceDist(gen));
    }
}

std::unique_ptr<Agent> ConsumerManager::factory() {
    auto state = std::make_shared<ConsumerState>();
    state->asset = asset;
    state->hungerDelay = clampTickSample(hungerDelayDist(gen));
    state->maxPrice = clampInt32Sample(maxPriceDist(gen));
    states.push_back(state);
    return std::make_unique<Consumer>(state);
}

void ConsumerManager::changeNumAgents(std::uint32_t numAgents) {
    std::int64_t diff = static_cast<std::int64_t>(numAgents) - static_cast<std::int64_t>(states.size());
    std::vector<std::int64_t> doomedIds;

    while (diff > 0) { create(); --diff; }

    while (diff < 0) {
        doomedIds.push_back(traderIdsUnderMgmt.back());
        traderIdsUnderMgmt.pop_back();
        states.pop_back();
        ++diff;
    }

    if (!doomedIds.empty())
        abm->removeAgents(doomedIds);
}

// ProducerManager

ProducerManager::ProducerManager(
    std::shared_ptr<ABM> abm_,
    std::string name_,
    std::string asset_)
    : AgentManager(std::move(abm_), std::move(name_)),
      asset(std::move(asset_)),
      preferedPriceDist(0, 0)
{}

void ProducerManager::changePreferedPrice(std::int32_t mean, std::int32_t std) {
    preferedPriceDist = std::normal_distribution<double>(
        static_cast<double>(mean), static_cast<double>(std));
    resampleAgentPreferedPrice();
}

void ProducerManager::resampleAgentPreferedPrice() {
    for (auto& state : states) {
        state->preferedPrice = clampInt32Sample(preferedPriceDist(gen));
    }
}

std::unique_ptr<Agent> ProducerManager::factory() {
    auto state = std::make_shared<ProducerState>();
    state->asset = asset;
    state->preferedPrice = clampInt32Sample(preferedPriceDist(gen));
    states.push_back(state);
    return std::make_unique<Producer>(state);
}

void ProducerManager::changeNumAgents(std::uint32_t numAgents) {
    std::int64_t diff = static_cast<std::int64_t>(numAgents) - static_cast<std::int64_t>(states.size());
    std::vector<std::int64_t> doomedIds;

    while (diff > 0) { create(); --diff; }

    while (diff < 0) {
        doomedIds.push_back(traderIdsUnderMgmt.back());
        traderIdsUnderMgmt.pop_back();
        states.pop_back();
        ++diff;
    }

    if (!doomedIds.empty())
        abm->removeAgents(doomedIds);
}

// ManufacturerManager

class ManufacturerManagerTickCallback : public TickCallback {
    ManufacturerManager* manager;
    public:
        explicit ManufacturerManagerTickCallback(ManufacturerManager* manager_)
            : manager(manager_)
        {}
        void callBackAction() override {
            if (manager == nullptr || manager->numAgentsFixed) return;
            manager->changeNumAgents(manager->newAgentPopulation());
        }
};

ManufacturerManager::ManufacturerManager(
    std::shared_ptr<ABM> abm_,
    std::string name_,
    Recipe recipe_)
    : AgentManager(std::move(abm_), std::move(name_)),
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

std::unique_ptr<Agent> ManufacturerManager::factory() {
    auto state = std::make_shared<ManufacturerState>(ManufacturerState{
        recipe,
        Inventory{},
        neutralAge,  // initialize to neutral age so new agents aren't immediately culled
        {}
    });
    states.push_back(state);
    return std::make_unique<Manufacturer>(state);
}

void ManufacturerManager::changeNumAgents(std::uint32_t numAgents) {
    std::int64_t diff = static_cast<std::int64_t>(numAgents) - static_cast<std::int64_t>(states.size());
    std::vector<std::int64_t> doomedIds;

    while (diff > 0) { create(); --diff; }

    while (diff < 0) {
        doomedIds.push_back(traderIdsUnderMgmt.back());
        traderIdsUnderMgmt.pop_back();
        states.pop_back();
        ++diff;
    }

    if (!doomedIds.empty())
        abm->removeAgents(doomedIds);
}

std::uint32_t ManufacturerManager::newAgentPopulation() {
    const std::int64_t currentCount = static_cast<std::int64_t>(states.size());

    if (numAgentsFixed || currentCount <= 0) {
        return static_cast<std::uint32_t>(std::max<std::int64_t>(0, currentCount));
    }

    std::int64_t recentSaleCount = 0;
    std::int64_t staleCount = 0;

    for (const auto& state : states) {
        if (state->timeSinceLastSale < neutralAge) {
            ++recentSaleCount;
        } else if (state->timeSinceLastSale >= staleAge) {
            ++staleCount;
        }
    }

    const double rawDiff =
        static_cast<double>(recentSaleCount - staleCount) * numAgentsScaleFactor;

    std::int64_t agentDiff = static_cast<std::int64_t>(std::round(rawDiff));
    if (agentDiff == 0 && recentSaleCount != staleCount) {
        // Prevent getting stuck at low populations
        agentDiff = (recentSaleCount > staleCount) ? 1 : -1;
    }

    return static_cast<std::uint32_t>(std::max<std::int64_t>(1, currentCount + agentDiff));
}

// PersonManager

class PersonManagerTickCallback : public TickCallback {
    PersonManager* manager;
    public:
        explicit PersonManagerTickCallback(PersonManager* manager_)
            : manager(manager_)
        {}
        void callBackAction() override {
            manager->birthNewAgents(manager->numBirths());
        }
};

PersonManager::PersonManager(
    std::shared_ptr<ABM> abm_,
    std::string name_)
    : AgentManager(std::move(abm_), std::move(name_))
{
    tickCallbackRegistration = abm->addTickCallback(
        std::make_unique<PersonManagerTickCallback>(this));
}

PersonManager::~PersonManager() {
    if (tickCallbackRegistration != nullptr) {
        abm->removeTickCallback(tickCallbackRegistration);
        tickCallbackRegistration = nullptr;
    }
}

std::unique_ptr<Agent> PersonManager::factory() {
    auto state = std::make_shared<PersonState>(PersonState{
        desires,
        spendingPower,
        lifeSpan
    });
    states.push_back(state);
    return std::make_unique<Person>(state);
}

void PersonManager::birthNewAgents(std::uint32_t births) {
    for (std::uint32_t i = 0; i < births; ++i) {
        create();
    }
}

std::uint32_t PersonManager::numBirths() {
    float growthDecay = std::min(1.0f, static_cast<float>(malthusFactor) / static_cast<float>(population));
    float growthProportion = popGrowthPerTick * growthDecay;
    return static_cast<std::uint32_t>(population * growthProportion);
}
