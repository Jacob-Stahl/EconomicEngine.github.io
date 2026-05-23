#include "consumer.h"

Consumer::Consumer(std::shared_ptr<ConsumerState> state_)
    : Agent(), state(std::move(state_))
{}

Action Consumer::policy(const Observation& observation){
    
    // Get new limit price based on current hunger
    auto price = std::min<std::int64_t>(
        std::max<std::int64_t>(0l, (std::int64_t)state->sinceLastFill - (std::int64_t)state->hungerDelay), 
        state->maxPrice);
    ++state->sinceLastFill;

    // qty always set to 1 to avoid partial fills
    Order order = orderBuilder
        .limit(BUY, price, 1)
        .withAsset(state->asset)
        .withTraderId(traderId)
        .withOrdId(-1)
        .build();

    if (state->orderOnBookId > 0) {
        return actionBuilder
            .withOrder(order)
            .withCancellation(state->orderOnBookId)
            .build();
    } else {
        return actionBuilder
            .withOrder(order)
            .build();
    }
}

void Consumer::orderPlaced(std::int64_t orderId, const tick now) {
    state->orderOnBookId = orderId;
}

void Consumer::orderCanceled(std::int64_t orderId, const tick now){
    state->orderOnBookId = 0;
}

void Consumer::matchFound(const Match& match, const tick now) {
    state->sinceLastFill = tick{0};
    state->orderOnBookId = 0;
}

Action Consumer::lastWill(const Observation& observation){
    return actionBuilder
        .withCancellation(state->orderOnBookId) // Cancel remaining order on book before death
        .build();
}