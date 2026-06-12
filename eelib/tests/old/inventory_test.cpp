#include <gtest/gtest.h>

#include "../agent.h"

struct InventoryTest : ::testing::Test {
    static constexpr long kTraderId = 42;
    static constexpr long kCounterpartyId = 7;

    Inventory inventory{};
    long nextOrderId = 1;

    Order makeOrder(
        long traderId,
        Side side,
        const std::string& asset,
        long qty,
        unsigned short price) {
        Order order(asset, side, LIMIT, price, qty);
        order.traderId = traderId;
        order.ordId = nextOrderId++;
        return order;
    }

    Match makeMatch(
        long buyerId,
        long sellerId,
        const std::string& asset,
        long orderQty,
        unsigned short price,
        long matchedQty) {
        Order buy = makeOrder(buyerId, BUY, asset, orderQty, price);
        Order sell = makeOrder(sellerId, SELL, asset, orderQty, price);
        return Match(buy, sell, matchedQty, price);
    }
};

TEST_F(InventoryTest, DirectUpdateTracksAssetAndCash) {
    inventory.update("ORE", 3, -150, kTraderId);

    EXPECT_EQ(inventory.qty("ORE"), 3);
    EXPECT_EQ(inventory.cash(), -150);
}

TEST_F(InventoryTest, DirectUpdateAccumulatesExistingPosition) {
    inventory.update("ORE", 3, -150, kTraderId);
    inventory.update("ORE", -1, 50, kTraderId);

    EXPECT_EQ(inventory.qty("ORE"), 2);
    EXPECT_EQ(inventory.cash(), -100);
}

TEST_F(InventoryTest, MissingAssetDefaultsToZero) {
    EXPECT_EQ(inventory.qty("MISSING"), 0);
    EXPECT_EQ(inventory.cash(), 0);
}

TEST_F(InventoryTest, BuyerUpdateUsesMatchedQuantity) {
    Match match = makeMatch(kTraderId, kCounterpartyId, "ORE", 10, 50, 3);

    inventory.update(match, kTraderId);

    EXPECT_EQ(inventory.qty("ORE"), 3);
}

TEST_F(InventoryTest, SellerUpdateUsesMatchedQuantity) {
    inventory.update("ORE", 5, 0, kTraderId);
    Match match = makeMatch(kCounterpartyId, kTraderId, "ORE", 8, 50, 2);

    inventory.update(match, kTraderId);

    EXPECT_EQ(inventory.qty("ORE"), 3);
}

TEST_F(InventoryTest, UnrelatedMatchDoesNotChangeInventory) {
    inventory.update("ORE", 5, 0, kTraderId);
    Match match = makeMatch(99, kCounterpartyId, "ORE", 4, 50, 2);

    inventory.update(match, kTraderId);

    EXPECT_EQ(inventory.qty("ORE"), 5);
    EXPECT_EQ(inventory.cash(), 0);
}