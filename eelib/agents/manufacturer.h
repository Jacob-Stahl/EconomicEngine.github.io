#pragma once
#include "agent.h"

struct ManufacturerState {
    Recipe recipe;
    Inventory inventory{};
    tick timeSinceLastSale = tick(0);

    // Asset -> OrderId placed on book
    std::map<std::string, std::int64_t> placedOrders;
};

class Manufacturer : public Agent {
    private:
        std::shared_ptr<ManufacturerState> state;

        /// @brief Estimated cost to produce one batch of the recipe at current bid prices.
        std::int64_t costOfProd(const Recipe& recipe, const Observation& observation);

        /// @brief Estimated revenue from selling one batch of outputs at current bid prices.
        std::int64_t saleRevenue(const Recipe& recipe, const Observation& observation);

        /// @brief Place limit buy orders for any recipe inputs not yet in inventory.
        std::vector<Order> procurementOrders(const Recipe& recipe, const Observation& observation);

        /// @brief Craft as many batches as possible from available inventory.
        void craft();

        /// @brief Sell all finished outputs in inventory via market orders.
        std::vector<Order> sellOrders();

    public:
        Manufacturer(std::shared_ptr<ManufacturerState> state_);
        Action policy(const Observation& observation) override;
        void matchFound(const Match& match, const tick now) override;
        void orderPlaced(std::int64_t orderId, const tick now) override;
        void orderCanceled(std::int64_t orderId, const tick now) override;
        Action lastWill(const Observation& observation) override;
};
