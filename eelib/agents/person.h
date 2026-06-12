#pragma once
#include "agent.h"

struct Desire {
    std::string asset;
    tick deathTheshhold = tick(0);
    tick ticksSinceLastConsumption = tick(0);

    float proportionToDeath() const;
};

struct PersonState {
    std::vector<Desire> desires;

    /// @brief the highest price an agent is able to bid for a desire
    std::int32_t spendingPower = 0;
    tick lifeSpan = tick(0);

    tick age = tick(0);
    std::int64_t lastPlacedBuyId = -1;

    bool shouldDie() const;
    void incrementAllDesireTicks();
};

class Person : public Agent {
    std::shared_ptr<PersonState> state;

    public:
        Person(std::shared_ptr<PersonState> state_);

        Action policy(const Observation& observation) override;
        void orderPlaced(std::int64_t orderId, const tick now) override;
        void matchFound(const Match& match, const tick now) override;
        void orderCanceled(std::int64_t orderId, const tick now) override;
        Action lastWill(const Observation& observation) override;

    /*

    Hungry for multiple assets.
    // Basic needs. Steep hunger curve. Agent dies if it goes a certain amount of ticks without consuming all of these
        - WATER
        - PROTIEN
        - CARBS
        - SUGAR

    // ?? With limited money, how to agents prioritize what to purchase? what they are most hungry for at this instant? what they run out of first?

    // Maslows Hierarchy of needs. https://en.wikipedia.org/wiki/Maslow's_hierarchy_of_needs#/media/File:Maslow's_Hierarchy_of_Needs_Pyramid_(original_five-level_model).png
    // ?? I suppose "assets" higher up on the pyramid could have flatter hunger curves.  Do agents die without esteem? can ESTEEM be a modeled as a commodity?

    */
};
