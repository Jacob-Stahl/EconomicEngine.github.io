#include "agent.h"
#include <string>
#include "tick.h"
#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>

void from_json(const nlohmann::json& j, Recipe& recipe) {
    j.at("inputs").get_to(recipe.inputs);
    j.at("outputs").get_to(recipe.outputs);
    recipe.cost = j.value("cost", 0L);
}

void from_json(const nlohmann::json& j, Desire& desire) {
    j.at("asset").get_to(desire.asset);
    desire.deathTheshhold = tick(j.at("deathThreshold").get<tick::rep>());
    desire.ticksSinceLastConsumption = tick(j.value("ticksSinceLastConsumption", tick::rep{0}));
}

std::vector<Desire> parseDesiresJson(const std::string& jsonText) {
    const nlohmann::json parsed = nlohmann::json::parse(jsonText);

    if (parsed.is_array()) {
        return parsed.get<std::vector<Desire>>();
    }

    if (parsed.is_object()) {
        return {parsed.get<Desire>()};
    }

    throw nlohmann::json::type_error::create(
        302,
        "desire JSON must be an object or array",
        &parsed);
}

std::vector<Recipe> parseRecipesJson(const std::string& jsonText) {
    const nlohmann::json parsed = nlohmann::json::parse(jsonText);

    if (parsed.is_array()) {
        return parsed.get<std::vector<Recipe>>();
    }

    if (parsed.is_object()) {
        return {parsed.get<Recipe>()};
    }

    throw nlohmann::json::type_error::create(
        302,
        "recipe JSON must be an object or array",
        &parsed);
}

Agent::Agent(long id) : traderId(id) {}

Action Agent::policy(const Observation& observation) {
    return Action();
}

// Consumer Implementation

Consumer::Consumer(long traderId_, std::shared_ptr<ConsumerState> state_)
    : Agent(traderId_), state(std::move(state_))
{}

Consumer::Consumer(long traderId_, std::string asset_, unsigned short maxPrice_,
    tick hungerDelay_)
    : Agent(traderId_), state(std::make_shared<ConsumerState>())
{
    state->asset = std::move(asset_);
    state->maxPrice = maxPrice_;
    state->hungerDelay = hungerDelay_;
}

Action Consumer::policy(const Observation& observation){
    
    // Get new limit price based on current hunger
    auto price = std::min(
        std::max(0l, (long)state->sinceLastFill.raw() - (long)state->hungerDelay.raw()
    ), (long)state->maxPrice);
    ++state->sinceLastFill;

    // qty always set to 1 to avoid partial fills
    Order order(state->asset, BUY, LIMIT, price, 1);
    
    if (state->orderOnBookId > 0) {
        return Action{order, state->orderOnBookId};
    } else {
        return Action{order};
    }
}

void Consumer::orderPlaced(long orderId, const tick now) {
    state->orderOnBookId = orderId;
}

void Consumer::orderCanceled(long orderId, const tick now){
    state->orderOnBookId = 0;
}

void Consumer::matchFound(const Match& match, const tick now) {
    state->sinceLastFill = tick{0};
    state->orderOnBookId = 0;
}

Action Consumer::lastWill(const Observation& observation){
    return Action(state->orderOnBookId); // Cancel order before death
}


// Producer Implementation

Producer::Producer(long traderId_, std::shared_ptr<ProducerState> state_)
    : Agent(traderId_), state(std::move(state_))
{}

Producer::Producer(long traderId_, std::string asset_, unsigned short preferedPrice_)
    : Agent(traderId_), state(std::make_shared<ProducerState>())
{
    state->asset = std::move(asset_);
    state->preferedPrice = preferedPrice_;
}

Action Producer::policy(const Observation& observation) {
    auto it = observation.assetObservations.find(state->asset);

    // If asset spread is missing, trust a new orderbook is created for new asset
    Spread assetSpread = Spread();
    if (it != observation.assetObservations.end()) {
        assetSpread = it->second.spread;
    }

    // Reduce production if bids are missing
    if(assetSpread.bidsMissing){
        if (state->qtyPerTick > 0)
            --state->qtyPerTick;
    }
    else if(assetSpread.highestBid > state->preferedPrice){
        ++state->qtyPerTick;
    }
    else if(assetSpread.highestBid < state->preferedPrice){
        if (state->qtyPerTick > 0)
            --state->qtyPerTick;
    }

    Order order(state->asset, SELL, MARKET, 0, state->qtyPerTick);
    return Action{order};
}

// Manufacturer Implementation
void Inventory::update(const Match& match, long thisTraderId){
    if(match.buyer.traderId == thisTraderId){
        auto asset = match.buyer.asset;
        update(asset, 
            (match.qty), 
            match.buyer.price * match.qty,
            thisTraderId
        );
    }
    if(match.seller.traderId == thisTraderId){
        auto asset = match.seller.asset;
        update(asset, 
            -(match.qty),
            -match.seller.price * match.qty,
            thisTraderId
        );
    }
};

void Inventory::update(
    const std::string& asset,
    int qtyChange,
    long cashChange,
    long thisTraderId){
    (void)thisTraderId;
    if(assets.find(asset) == assets.end()){
        assets.emplace(asset, qtyChange);
    }
    else{
        int newQty = assets.at(asset) + qtyChange;
        assets[asset] = newQty;
    }
    cashBalance += cashChange;
}

Manufacturer::Manufacturer(long traderId_, std::shared_ptr<ManufacturerState> state_)
    : Agent(traderId_), state(std::move(state_))
{}

long Manufacturer::costOfProd(
    const Recipe& recipe,
    const Observation& observation) {
    long totalCost = recipe.cost;

    for (const auto& [asset, qty] : recipe.inputs) {
        auto spreadIt = observation.assetObservations.find(asset);
        unsigned short bidPrice = 1;

        if (spreadIt != observation.assetObservations.end() && !spreadIt->second.spread.bidsMissing) {
            bidPrice = static_cast<unsigned short>(std::min<long>(

                // Add +1 tp bid price. We want to out bid the other buy limits.
                // This puts natural upward pressure on the price.
                static_cast<long>(spreadIt->second.spread.highestBid) + 1,
                std::numeric_limits<unsigned short>::max()));
        }

        totalCost += static_cast<long>(qty) * bidPrice;
    }

    return totalCost;
}

long Manufacturer::saleRevenue(
    const Recipe& recipe,
    const Observation& observation) {
    long totalRevenue = 0;

    for (const auto& [asset, qty] : recipe.outputs) {
        auto spreadIt = observation.assetObservations.find(asset);
        unsigned short salePrice = 0;

        if (spreadIt != observation.assetObservations.end() && !spreadIt->second.spread.bidsMissing) {
            salePrice = spreadIt->second.spread.highestBid;
        }

        totalRevenue += static_cast<long>(qty) * salePrice;
    }

    return totalRevenue;
}

std::vector<Order> Manufacturer::procurementOrders(
    const Recipe& recipe,
    const Observation& observation) {
    std::vector<Order> orders;
    orders.reserve(recipe.inputs.size());

    for (const auto& [asset, requiredQty] : recipe.inputs) {
        long deficit = static_cast<long>(requiredQty) - state->inventory.qty(asset);
        if (deficit <= 0) {
            continue;
        }

        unsigned short bidPrice = 1;
        auto spreadIt = observation.assetObservations.find(asset);
        if (spreadIt != observation.assetObservations.end() && !spreadIt->second.spread.bidsMissing) {
            bidPrice = static_cast<unsigned short>(std::min<long>(
                static_cast<long>(spreadIt->second.spread.highestBid) + 1,
                std::numeric_limits<unsigned short>::max()));
        }

        orders.emplace_back(
            asset,
            BUY,
            LIMIT,
            bidPrice,
            static_cast<unsigned int>(std::min<long>(
                deficit,
                std::numeric_limits<unsigned int>::max())));
    }

    return orders;
}

void Manufacturer::craft() {
    if (state->recipe.inputs.empty()) {
        return;
    }

    long craftCount = std::numeric_limits<long>::max();
    bool hasPositiveInput = false;

    for (const auto& [asset, requiredQty] : state->recipe.inputs) {
        if (requiredQty <= 0) {
            continue;
        }

        hasPositiveInput = true;
        craftCount = std::min(
            craftCount,
            state->inventory.qty(asset) / static_cast<long>(requiredQty));
    }

    if (!hasPositiveInput || craftCount <= 0) {
        return;
    }

    for (const auto& [asset, requiredQty] : state->recipe.inputs) {
        if (requiredQty <= 0) {
            continue;
        }
        state->inventory.update(asset, -requiredQty * craftCount, 0, traderId);
    }

    for (const auto& [asset, producedQty] : state->recipe.outputs) {
        if (producedQty <= 0) {
            continue;
        }
        state->inventory.update(asset, producedQty * craftCount, 0, traderId);
    }
}

std::vector<Order> Manufacturer::sellOrders() {
    std::vector<Order> orders;
    orders.reserve(state->recipe.outputs.size());

    for (const auto& [asset, producedQty] : state->recipe.outputs) {
        (void)producedQty;
        long inventoryQty = state->inventory.qty(asset);
        if (inventoryQty <= 0) {
            continue;
        }

        orders.emplace_back(
            asset,
            SELL,
            MARKET,
            0,
            static_cast<unsigned int>(std::min<long>(
                inventoryQty,
                std::numeric_limits<unsigned int>::max())));
    }

    return orders;
}

Action Manufacturer::policy(const Observation& observation){
    long prodCost = costOfProd(state->recipe, observation);
    long expectedSaleRevenue = saleRevenue(state->recipe, observation);
    std::vector<Order> orders;

    // If sale revenue > cost, place bids for remaining required feedstock
    if (expectedSaleRevenue > prodCost) {
        orders = procurementOrders(state->recipe, observation);
    }

    craft();

    auto productOrders = sellOrders();
    orders.insert(orders.end(), productOrders.begin(), productOrders.end());

    ++state->timeSinceLastSale;

    return Action(std::move(orders));
}

void Manufacturer::orderPlaced(long orderId, const tick now) {
}

void Manufacturer::matchFound(const Match& match, const tick now) {
    if(match.seller.traderId == traderId){
        state->timeSinceLastSale = tick(0);
    }
    state->inventory.update(match, traderId);
}

void Manufacturer::orderCanceled(long orderId, const tick now) {
}

Action Manufacturer::lastWill(const Observation& observation) {
    std::vector<long> doomedOrderIds;
    doomedOrderIds.reserve(state->placedOrders.size());

    for (const auto& [asset, orderId] : state->placedOrders) {
        if (orderId > 0) {
            doomedOrderIds.push_back(orderId);
        }
    }

    return Action(std::move(doomedOrderIds));
}

// Person Implementation

float Desire::proportionToDeath() const{
    float sinceLastCons = (float)ticksSinceLastConsumption.raw();
    float thresh = (float)deathTheshhold.raw();

    if(thresh == 0){
        return 0;
    }

    return sinceLastCons / thresh;
}

bool PersonState::shouldDie() const{
    // Check for starvation
    for(const auto& desire : desires){
        if(desire.proportionToDeath() >= 1){
            return true;
        }
    }

    // Check for senescence
    if(lifeSpan > zeroTick() && age > lifeSpan){
        return true;
    }

    return false;
}

void PersonState::incrementAllDesireTicks(){
    for(auto& desire : desires){
        ++desire.ticksSinceLastConsumption;
    }
}

Action Person::policy(const Observation& observation){
    Action action{};

    // Age 1 tick. 
    // TODO Should something like this really be in the policy?
    ++state->age;
    
    // Cancel previous buy order, if any
    long lastPlacedBuyId = state->lastPlacedBuyId;
    if(lastPlacedBuyId != -1){
        action.addCancellation(lastPlacedBuyId);
    }

    // SELL MARKET 1 unit of labor
    Order sell(
        "LABOR",
        SELL,
        MARKET, 
        0, 
        1
    );
    action.addOrder(sell);

    // Don't place buys if there are no desires
    if(state->desires.size() == 0){
        return action;
    }

    // BUY LIMIT 1 unit of Desire with highest deathProportion
    auto mostDesired = std::max_element(
        state->desires.begin(), state->desires.end(),
            [](const Desire& a, const Desire& b){
                return a.proportionToDeath() < b.proportionToDeath();
            });

    unsigned short price = mostDesired->proportionToDeath() * state->spendingPower;
    Order buy(
        mostDesired->asset,
        BUY,
        LIMIT,
        price,
        1
    );
    action.addOrder(buy);
    state->incrementAllDesireTicks();
    return action;
};

void Person::matchFound(const Match& match, const tick now){
    // Handle bids for desires
    if(match.buyer.traderId == traderId){
        // Find which desired asset was matched
        // Reset time since last consumption
        for(auto& desire : state->desires){
            if(match.buyer.asset == desire.asset){
                desire.ticksSinceLastConsumption = tick(0);
            };
        }
        return;
    }

    // Handle sale of LABOR
    if(match.seller.traderId == traderId){
/*
    It could be interesting to dynamically adjust spendingPower 
    based on the LABOR sale price (wage)
    
    NOP for now.
*/
    }
};

void Person::orderPlaced(long orderId, const tick now){
    state->lastPlacedBuyId = orderId;
};

void Person::orderCanceled(long orderId, const tick now){
    state->lastPlacedBuyId = -1;
}

Action Person::lastWill(const Observation& observation){
    long lastPlacedBuyId = state->lastPlacedBuyId;
    if(lastPlacedBuyId != -1){
        return Action(lastPlacedBuyId);
    }
    else{
        return Action();
    }
}