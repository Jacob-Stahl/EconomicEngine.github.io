#pragma once
#include "agent.h"

struct ProducerState{
    std::string asset;
    std::int32_t preferedPrice;
    std::uint32_t qtyPerTick = 0;
};

class Producer : public Agent{

    private:
        std::shared_ptr<ProducerState> state;

    public:
        Producer(
            std::int64_t traderId_,
            std::shared_ptr<ProducerState> state_);
        Action policy(const Observation& observation) override;
};
