#include <gtest/gtest.h>

#include "../agent.h"

static std::shared_ptr<ManufacturerState> makeManufacturerState(long traderId) {
    (void)traderId;
    return std::shared_ptr<ManufacturerState>(
        new ManufacturerState{Recipe(), Inventory(), tick(0), {}});
}

TEST(ManufacturerTest, PolicyPlacesProcurementOrdersWhenProductionIsProfitable) {
    auto state = makeManufacturerState(1);
    state->recipe = Recipe({{"ORE", 2}}, {{"METAL", 1}}, 5);

    Manufacturer manufacturer(1, state);

    Observation observation;
    observation.assetObservations["ORE"].spread = Spread{false, true, 4, 0};
    observation.assetObservations["METAL"].spread = Spread{false, true, 20, 0};

    Action action = manufacturer.policy(observation);

    ASSERT_EQ(action.ordersToPlace.size(), 1u);
    EXPECT_TRUE(action.orderIdsToCancel.empty());

    const Order& procurement = action.ordersToPlace[0];
    EXPECT_EQ(procurement.asset, "ORE");
    EXPECT_EQ(procurement.side, BUY);
    EXPECT_EQ(procurement.type, LIMIT);
    EXPECT_EQ(procurement.price, 5);
    EXPECT_EQ(procurement.qty, 2u);

    EXPECT_EQ(state->inventory.qty("ORE"), 0);
    EXPECT_EQ(state->inventory.qty("METAL"), 0);
    EXPECT_EQ(state->timeSinceLastSale, tick(1));
}

TEST(ManufacturerTest, PolicyCraftsInventoryAndPlacesSellOrdersForOutputs) {
    auto state = makeManufacturerState(1);
    state->recipe = Recipe({{"ORE", 2}}, {{"METAL", 1}}, 100);

    Manufacturer manufacturer(1, state);
    state->inventory.update("ORE", 4, 0, manufacturer.traderId);

    Observation observation;
    observation.assetObservations["METAL"].spread = Spread{false, true, 1, 0};

    Action action = manufacturer.policy(observation);

    ASSERT_EQ(action.ordersToPlace.size(), 1u);
    EXPECT_TRUE(action.orderIdsToCancel.empty());

    const Order& sale = action.ordersToPlace[0];
    EXPECT_EQ(sale.asset, "METAL");
    EXPECT_EQ(sale.side, SELL);
    EXPECT_EQ(sale.type, MARKET);
    EXPECT_EQ(sale.qty, 2u);

    EXPECT_EQ(state->inventory.qty("ORE"), 0);
    EXPECT_EQ(state->inventory.qty("METAL"), 2);
    EXPECT_EQ(state->timeSinceLastSale, tick(1));
}

TEST(RecipeJsonTest, ParseRecipesJsonBuildsRecipeVectorFromString) {
    const std::string jsonText = R"json(
    [
        {
            "inputs": {"ORE": 2, "LABOR": 1},
            "outputs": {"METAL": 1},
            "cost": 15
        },
        {
            "inputs": {"SAND": 3},
            "outputs": {"GLASS": 2}
        }
    ]
    )json";

    const std::vector<Recipe> recipes = parseRecipesJson(jsonText);

    ASSERT_EQ(recipes.size(), 2u);
    EXPECT_EQ(recipes[0].inputs.at("ORE"), 2);
    EXPECT_EQ(recipes[0].inputs.at("LABOR"), 1);
    EXPECT_EQ(recipes[0].outputs.at("METAL"), 1);
    EXPECT_EQ(recipes[0].cost, 15);
    EXPECT_EQ(recipes[1].inputs.at("SAND"), 3);
    EXPECT_EQ(recipes[1].outputs.at("GLASS"), 2);
    EXPECT_EQ(recipes[1].cost, 0);
}