#pragma once
#include "agent.h"

struct ConsumerState{
    std::string asset;
    tick sinceLastFill = tick(0);
    std::int64_t orderOnBookId = 0;
    std::int32_t maxPrice;
    tick hungerDelay = tick(0);
};

class Consumer : public Agent{

    private:
        std::shared_ptr<ConsumerState> state;

    public:
        Consumer(std::shared_ptr<ConsumerState> state_);
        Action policy(const Observation& observation) override;
        void orderPlaced(std::int64_t orderId, const tick now) override;
        void matchFound(const Match& match, const tick now) override;
        void orderCanceled(std::int64_t orderId, const tick now) override;
        Action lastWill(const Observation& observation) override;
};
