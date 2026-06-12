#include "producer.h"

Producer::Producer(std::shared_ptr<ProducerState> state_)
    : Agent(), state(std::move(state_))
{}

Action Producer::policy(const Observation& observation) {
    auto it = observation.assetObservations.find(state->asset);

    // If asset spread is missing, assume a new orderbook was created for the asset
    Spread assetSpread = Spread();
    if (it != observation.assetObservations.end()) {
        assetSpread = it->second.spread;
    }

    // Reduce production if bids are missing
    if (assetSpread.bidsMissing) {
        if (state->qtyPerTick > 0)
            --state->qtyPerTick;
    }
    else if (assetSpread.highestBid > state->preferedPrice) {
        ++state->qtyPerTick;
    }
    else if (assetSpread.highestBid < state->preferedPrice) {
        if (state->qtyPerTick > 0)
            --state->qtyPerTick;
    }

    if (state->qtyPerTick == 0) return actionBuilder.build();

    Order order = orderBuilder
        .market(SELL, state->qtyPerTick)
        .withAsset(state->asset)
        .withTraderId(traderId)
        .withIncrementedOrderId()
        .build();

    return actionBuilder
        .withOrder(order)
        .build();
}
