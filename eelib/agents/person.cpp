#include "person.h"
#include <algorithm>

// Desire

float Desire::proportionToDeath() const {
    float sinceLastCons = static_cast<float>(ticksSinceLastConsumption);
    float thresh = static_cast<float>(deathTheshhold);

    if (thresh == 0) {
        return 0;
    }

    return sinceLastCons / thresh;
}

// PersonState

bool PersonState::shouldDie() const {
    // Check for starvation
    for (const auto& desire : desires) {
        if (desire.proportionToDeath() >= 1) {
            return true;
        }
    }

    // Check for senescence
    if (lifeSpan > 0 && age > lifeSpan) {
        return true;
    }

    return false;
}

void PersonState::incrementAllDesireTicks() {
    for (auto& desire : desires) {
        ++desire.ticksSinceLastConsumption;
    }
}

// Person

Person::Person(std::int64_t traderId_, std::shared_ptr<PersonState> state_)
    : Agent(traderId_), state(std::move(state_))
{}

Action Person::policy(const Observation& observation) {
    // Age 1 tick.
    // TODO Should something like this really be in the policy?
    ++state->age;

    // Cancel previous buy order, if any
    if (state->lastPlacedBuyId != -1) {
        actionBuilder.withCancellation(state->lastPlacedBuyId);
    }

    // SELL MARKET 1 unit of labor
    Order sell = OrderBuilder()
        .market(SELL, 1)
        .withAsset("LABOR")
        .withTraderId(traderId)
        .withOrdId(-1)
        .build();
    actionBuilder.withOrder(sell);

    // Don't place buys if there are no desires
    if (state->desires.empty()) {
        return actionBuilder.build();
    }

    // BUY LIMIT 1 unit of the desire with the highest death proportion
    auto mostDesired = std::max_element(
        state->desires.begin(), state->desires.end(),
        [](const Desire& a, const Desire& b) {
            return a.proportionToDeath() < b.proportionToDeath();
        });

    std::int32_t price = static_cast<std::int32_t>(mostDesired->proportionToDeath() * state->spendingPower);
    Order buy = OrderBuilder()
        .limit(BUY, price, 1)
        .withAsset(mostDesired->asset)
        .withTraderId(traderId)
        .withOrdId(-1)
        .build();
    actionBuilder.withOrder(buy);

    state->incrementAllDesireTicks();
    return actionBuilder.build();
}

void Person::orderPlaced(std::int64_t orderId, const tick now) {
    state->lastPlacedBuyId = orderId;
}

void Person::orderCanceled(std::int64_t orderId, const tick now) {
    state->lastPlacedBuyId = -1;
}

void Person::matchFound(const Match& match, const tick now) {
    // Handle bids for desires
    if (match.buyer.traderId == traderId) {
        // Reset time since last consumption for the matched desire
        for (auto& desire : state->desires) {
            if (match.buyer.asset == desire.asset) {
                desire.ticksSinceLastConsumption = tick(0);
            }
        }
        return;
    }

    // Handle sale of LABOR
    if (match.seller.traderId == traderId) {
        /*
        It could be interesting to dynamically adjust spendingPower
        based on the LABOR sale price (wage)

        NOP for now.
        */
    }
}

Action Person::lastWill(const Observation& observation) {
    if (state->lastPlacedBuyId != -1) {
        return actionBuilder
            .withCancellation(state->lastPlacedBuyId)
            .build();
    }
    return actionBuilder.build();
}
