#include <gtest/gtest.h>
#include "../matcher.h"
#include "../notifier.h"
#include "../order.h"

class MatcherTest : public ::testing::Test {
protected:
    Matcher matcher;

    void SetUp() override {}

    Order makeLimit(long ordId, Side side, int price, unsigned int qty) {
        return OrderBuilder()
            .limit(side, price, qty)
            .withAsset("TEST")
            .withTraderId(1)
            .withOrdId(ordId)
            .build();
    }

    Order makeMarket(long ordId, Side side, unsigned int qty) {
        return OrderBuilder()
            .market(side, qty)
            .withAsset("TEST")
            .withTraderId(1)
            .withOrdId(ordId)
            .build();
    }

    Order makeStop(long ordId, Side side, int stopPrice, unsigned int qty) {
        return OrderBuilder()
            .stop(side, stopPrice, qty)
            .withAsset("TEST")
            .withTraderId(1)
            .withOrdId(ordId)
            .build();
    }

    Order makeStopLimit(long ordId, Side side, int limitPrice, int stopPrice, unsigned int qty) {
        return OrderBuilder()
            .stopLimit(side, limitPrice, stopPrice, qty)
            .withAsset("TEST")
            .withTraderId(1)
            .withOrdId(ordId)
            .build();
    }
};

TEST_F(MatcherTest, PlaceBuyAndSellLimits_NoMatch_StateIsCorrect) {
    // Arrange & Act

    matcher.placeOrder(makeLimit(1, BUY, 100, 1));
    matcher.placeOrder(makeLimit(2, SELL, 110, 1));
    matcher.placeOrder(makeLimit(3, BUY, 90, 1));
    matcher.placeOrder(makeLimit(4, SELL, 120, 1));


    const Spread& spread = matcher.getSpread();

    // Assert

    // No matches or cancellations expected
    EXPECT_EQ(0, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // 4 orders have been registered
    EXPECT_EQ(4, matcher.notifier->orderRegistery.size());

    // check spread
    EXPECT_FALSE(spread.bidsMissing);
    EXPECT_FALSE(spread.asksMissing);
    EXPECT_EQ(spread.highestBid, 100);
    EXPECT_EQ(spread.lowestAsk, 110);
}

TEST_F(MatcherTest, PlaceBuyAndSellLimits_SpreadCrossed_StateIsCorrect){
    // Arrange & Act
    matcher.placeOrder(makeLimit(1, BUY, 100, 1));
    matcher.placeOrder(makeLimit(2, SELL, 110, 1));
    matcher.placeOrder(makeLimit(3, BUY, 111, 1));
    matcher.placeOrder(makeLimit(4, SELL, 99, 1));

    const Spread& spread = matcher.getSpread();

    // Expect 2 matches. No cancellations expected
    EXPECT_EQ(2, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // check spread
    EXPECT_TRUE(spread.bidsMissing);
    EXPECT_TRUE(spread.asksMissing);

    // Check matches

    EXPECT_EQ(3, matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1, matcher.notifier->matches[0].qty);
    EXPECT_EQ(110, matcher.notifier->matches[0].price);

    EXPECT_EQ(1, matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(4, matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(1, matcher.notifier->matches[1].qty);
    EXPECT_EQ(100, matcher.notifier->matches[1].price);
}

TEST_F(MatcherTest, PlaceBuyAndSellLimitsAndMarkets_SpreadCrossed_StateIsCorrect){
    // Arrange & Act
    matcher.placeOrder(makeLimit(1, BUY, 100, 1));
    matcher.placeOrder(makeLimit(2, SELL, 110, 1));
    matcher.placeOrder(makeMarket(3, BUY, 1));
    matcher.placeOrder(makeMarket(4, SELL, 1));

    const Spread& spread = matcher.getSpread();

    // Expect 2 matches. No cancellations expected
    EXPECT_EQ(2, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // check spread
    EXPECT_TRUE(spread.bidsMissing);
    EXPECT_TRUE(spread.asksMissing);

    // Check matches
    EXPECT_EQ(3, matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1, matcher.notifier->matches[0].qty);
    EXPECT_EQ(110, matcher.notifier->matches[0].price);

    EXPECT_EQ(1, matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(4, matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(1, matcher.notifier->matches[1].qty);
    EXPECT_EQ(100, matcher.notifier->matches[1].price);
}

TEST_F(MatcherTest, CancelOrder_NotMatched_StateIsCorrect){
    matcher.placeOrder(makeLimit(1, BUY, 100, 1));
    matcher.placeOrder(makeLimit(2, SELL, 110, 1));

    // Cancel 2 then 1
    matcher.cancelOrder(2);
    matcher.cancelOrder(1);

    matcher.placeOrder(makeMarket(3, BUY, 1));
    matcher.placeOrder(makeMarket(4, SELL, 1));

    const Spread& spread = matcher.getSpread();

    // Expect no matches or cancellations expected
    EXPECT_EQ(0, matcher.notifier->matches.size());
    EXPECT_EQ(4, matcher.notifier->cancellations.size());

    // check spread
    EXPECT_TRUE(spread.bidsMissing);
    EXPECT_TRUE(spread.asksMissing);

    // Check cancellations. 
    // First 2 are cancelled manually, in correct order
    EXPECT_EQ(2, matcher.notifier->cancellations[0].ordId);
    EXPECT_EQ(1, matcher.notifier->cancellations[1].ordId);

    // Next 2 market orders are cancelled because there is no liquidity
    EXPECT_EQ(3, matcher.notifier->cancellations[2].ordId);
    EXPECT_EQ(4, matcher.notifier->cancellations[3].ordId);   
}

TEST_F(MatcherTest, PlaceBuyAndSellLimits_SpreadCrossed_LiquidityNotDrained_StateIsCorrect){
    // Arrange & Act
    matcher.placeOrder(makeLimit(1, BUY, 100, 2));
    matcher.placeOrder(makeLimit(2, SELL, 110, 2));
    matcher.placeOrder(makeMarket(3, BUY, 1));
    matcher.placeOrder(makeMarket(4, SELL, 1));

    const Spread& spread = matcher.getSpread();

    // Expect 2 matches. No cancellations expected
    EXPECT_EQ(2, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // check spread
    EXPECT_FALSE(spread.bidsMissing);
    EXPECT_FALSE(spread.asksMissing);

    EXPECT_EQ(100, spread.highestBid);
    EXPECT_EQ(110, spread.lowestAsk);

    // Check matches
    EXPECT_EQ(3, matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1, matcher.notifier->matches[0].qty);
    EXPECT_EQ(110, matcher.notifier->matches[0].price);

    EXPECT_EQ(1, matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(4, matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(1, matcher.notifier->matches[1].qty);
    EXPECT_EQ(100, matcher.notifier->matches[1].price);
}

TEST_F(MatcherTest, SpreadCrossed_PartialFill_BUY_LIMIT_PlacedOnBook_StateIsCorrect){
    // Arrange & Act
    matcher.placeOrder(makeLimit(1, SELL, 110, 1));
    matcher.placeOrder(makeLimit(2, SELL, 101, 2));
    matcher.placeOrder(makeLimit(3, SELL, 100, 2));

    matcher.placeOrder(makeLimit(4, BUY, 95, 2));
    matcher.placeOrder(makeLimit(5, BUY, 94, 2));
    matcher.placeOrder(makeLimit(6, BUY, 90, 1));

    matcher.placeOrder(makeLimit(7, BUY, 105, 5));

    const Spread& spread = matcher.getSpread();

    // Expect 2 matches. No cancellations expected
    EXPECT_EQ(2, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // check spread
    EXPECT_FALSE(spread.bidsMissing);
    EXPECT_FALSE(spread.asksMissing);

    EXPECT_EQ(105, spread.highestBid);
    EXPECT_EQ(110, spread.lowestAsk);

    // Check matches
    EXPECT_EQ(7, matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(3, matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[0].qty);
    EXPECT_EQ(100, matcher.notifier->matches[0].price);

    EXPECT_EQ(7, matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[1].qty);
    EXPECT_EQ(101, matcher.notifier->matches[1].price);
}

TEST_F(MatcherTest, SpreadCrossed_PartialFill_SELL_LIMIT_PlacedOnBook_StateIsCorrect){
    // Arrange & Act
    matcher.placeOrder(makeLimit(4, SELL, 105, 2));
    matcher.placeOrder(makeLimit(5, SELL, 106, 2));
    matcher.placeOrder(makeLimit(6, SELL, 110, 1));

    matcher.placeOrder(makeLimit(1, BUY, 90, 1));
    matcher.placeOrder(makeLimit(2, BUY, 99, 2));
    matcher.placeOrder(makeLimit(3, BUY, 100, 2));

    matcher.placeOrder(makeLimit(7, SELL, 95, 5));

    const Spread& spread = matcher.getSpread();

    // Expect 2 matches. No cancellations expected
    EXPECT_EQ(2, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // check spread
    EXPECT_FALSE(spread.bidsMissing);
    EXPECT_FALSE(spread.asksMissing);

    EXPECT_EQ(90, spread.highestBid);
    EXPECT_EQ(95, spread.lowestAsk);

    // Check matches
    EXPECT_EQ(3, matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(7, matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[0].qty);
    EXPECT_EQ(100, matcher.notifier->matches[0].price);

    EXPECT_EQ(2, matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(7, matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(2, matcher.notifier->matches[1].qty);
    EXPECT_EQ(99, matcher.notifier->matches[1].price);
}

TEST_F(MatcherTest, StopsActivateOnPriceSignal){

    // Place BUY LIMITS and a SELL STOP with a trigger price in the middle
    matcher.placeOrder(makeLimit(1, BUY, 115, 1));
    matcher.placeOrder(makeLimit(2, BUY, 111, 1));
    matcher.placeOrder(makeLimit(3, BUY, 110, 1));
    matcher.placeOrder(makeLimit(4, BUY, 105, 1));
    matcher.placeOrder(makeStop(5, SELL, 110, 1));

    // No matches expected initially
    EXPECT_EQ(0, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_FALSE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);
    EXPECT_EQ(115, matcher.getSpread().highestBid);

    // Take the BUY limit @115. 1 match expected, trigger price IS NOT reached
    matcher.placeOrder(makeMarket(6, SELL, 1));
    EXPECT_EQ(1, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_FALSE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);
    EXPECT_EQ(111, matcher.getSpread().highestBid);

    // Take the BUY limit @111 AND @110. another match expected, trigger price IS reached
    matcher.placeOrder(makeMarket(7, SELL, 1));
    EXPECT_EQ(3, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_FALSE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);
    EXPECT_EQ(105, matcher.getSpread().highestBid);

    // Take the BUY limit @105. Book should be empty afterwards
    matcher.placeOrder(makeMarket(8, SELL, 1));
    EXPECT_EQ(4, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);

    // Match 0: first market sell takes the best bid @115
    EXPECT_EQ(1,   matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(6,   matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[0].qty);
    EXPECT_EQ(115, matcher.notifier->matches[0].price);

    // Match 1: second market sell takes bid @111, activating the dormant stop at 110
    EXPECT_EQ(2,   matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(7,   matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[1].qty);
    EXPECT_EQ(111, matcher.notifier->matches[1].price);

    // Match 2: the now-active stop market-sells into the bid @110
    EXPECT_EQ(3,   matcher.notifier->matches[2].buyer.ordId);
    EXPECT_EQ(5,   matcher.notifier->matches[2].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[2].qty);
    EXPECT_EQ(110, matcher.notifier->matches[2].price);

    // Match 3: third market sell takes the remaining bid @105
    EXPECT_EQ(4,   matcher.notifier->matches[3].buyer.ordId);
    EXPECT_EQ(8,   matcher.notifier->matches[3].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[3].qty);
    EXPECT_EQ(105, matcher.notifier->matches[3].price);
}

TEST_F(MatcherTest, BuyStopLimitActivatesOnPriceSignal){

    // Place SELL LIMITS at 100, 105, 110, 115
    // and a dormant BUY STOP LIMIT (stopPrice=105, limitPrice=110)
    // The stop is dormant because stopPrice(105) > lowestAsk(100)
    matcher.placeOrder(makeLimit(1, SELL, 100, 1));
    matcher.placeOrder(makeLimit(2, SELL, 105, 1));
    matcher.placeOrder(makeLimit(3, SELL, 110, 1));
    matcher.placeOrder(makeLimit(4, SELL, 115, 1));
    matcher.placeOrder(makeStopLimit(5, BUY, 110, 105, 1));

    // No matches expected initially
    EXPECT_EQ(0, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_FALSE(matcher.getSpread().asksMissing);
    EXPECT_EQ(100, matcher.getSpread().lowestAsk);

    // Take the SELL limit @100. 2 matches expected: the market buy fills @100,
    // which sweeps through bid@105, activating the dormant stop.
    // The now-active stop limit immediately takes the ask @105 (within its limit of 110).
    matcher.placeOrder(makeMarket(6, BUY, 1));
    EXPECT_EQ(2, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_FALSE(matcher.getSpread().asksMissing);
    EXPECT_EQ(110, matcher.getSpread().lowestAsk);

    // Take the SELL limit @110. 1 more match expected, trigger price is NOT reached again
    matcher.placeOrder(makeMarket(7, BUY, 1));
    EXPECT_EQ(3, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_FALSE(matcher.getSpread().asksMissing);
    EXPECT_EQ(115, matcher.getSpread().lowestAsk);

    // Take the SELL limit @115. Book should be empty afterwards
    matcher.placeOrder(makeMarket(8, BUY, 1));
    EXPECT_EQ(4, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);

    // Match 0: first market buy takes the best ask @100
    EXPECT_EQ(6,   matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[0].qty);
    EXPECT_EQ(100, matcher.notifier->matches[0].price);

    // Match 1: the now-active stop limit buys into the ask @105 (within its limit of 110)
    EXPECT_EQ(5,   matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(2,   matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[1].qty);
    EXPECT_EQ(105, matcher.notifier->matches[1].price);

    // Match 2: second market buy takes ask @110
    EXPECT_EQ(7,   matcher.notifier->matches[2].buyer.ordId);
    EXPECT_EQ(3,   matcher.notifier->matches[2].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[2].qty);
    EXPECT_EQ(110, matcher.notifier->matches[2].price);

    // Match 3: third market buy takes ask @115
    EXPECT_EQ(8,   matcher.notifier->matches[3].buyer.ordId);
    EXPECT_EQ(4,   matcher.notifier->matches[3].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[3].qty);
    EXPECT_EQ(115, matcher.notifier->matches[3].price);
}

TEST_F(MatcherTest, StopChainReaction_ActivatedStopTriggersAnotherStop){

    // Setup: BUY LIMITS descending, two dormant SELL STOPs at intermediate prices.
    // Stop A (stopPrice=115) activates when highestBid falls to 115.
    // Stop B (stopPrice=105) activates when highestBid falls to 105.
    // Stop A's activation should drive price down far enough to trigger Stop B.
    matcher.placeOrder(makeLimit(1, BUY, 120, 1));
    matcher.placeOrder(makeLimit(2, BUY, 110, 1));
    matcher.placeOrder(makeLimit(3, BUY, 100, 1));
    matcher.placeOrder(makeLimit(4, BUY,  90, 1));
    matcher.placeOrder(makeStop(5, SELL, 115, 1)); // Stop A — dormant: 115 < highestBid(120)
    matcher.placeOrder(makeStop(6, SELL, 105, 1)); // Stop B — dormant: 105 < highestBid(120)

    // No matches expected initially
    EXPECT_EQ(0, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_FALSE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);
    EXPECT_EQ(120, matcher.getSpread().highestBid);

    // Market SELL #7 takes bid@120. While scanning, Stop A's bin@115 is hit → activates.
    // Stop A (now a market sell) takes bid@110. While scanning, Stop B's bin@105 is hit → activates.
    // Stop B (now a market sell) takes bid@100.
    // All three happen in a single placeOrder call via the while loop.
    matcher.placeOrder(makeMarket(7, SELL, 1));
    EXPECT_EQ(3, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_FALSE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);
    EXPECT_EQ(90, matcher.getSpread().highestBid);

    // Final market sell drains the book
    matcher.placeOrder(makeMarket(8, SELL, 1));
    EXPECT_EQ(4, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);

    // Match 0: market sell #7 takes best bid @120
    EXPECT_EQ(1,   matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(7,   matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[0].qty);
    EXPECT_EQ(120, matcher.notifier->matches[0].price);

    // Match 1: Stop A activates, market-sells into bid @110
    EXPECT_EQ(2,   matcher.notifier->matches[1].buyer.ordId);
    EXPECT_EQ(5,   matcher.notifier->matches[1].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[1].qty);
    EXPECT_EQ(110, matcher.notifier->matches[1].price);

    // Match 2: Stop B activates (triggered by Stop A's fill), market-sells into bid @100
    EXPECT_EQ(3,   matcher.notifier->matches[2].buyer.ordId);
    EXPECT_EQ(6,   matcher.notifier->matches[2].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[2].qty);
    EXPECT_EQ(100, matcher.notifier->matches[2].price);

    // Match 3: market sell #8 takes the last remaining bid @90
    EXPECT_EQ(4,   matcher.notifier->matches[3].buyer.ordId);
    EXPECT_EQ(8,   matcher.notifier->matches[3].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[3].qty);
    EXPECT_EQ(90,  matcher.notifier->matches[3].price);
}

TEST_F(MatcherTest, MarketAndActivatedStop_NoLiquidity_AreCancelled){

    // One bid exists. A dormant SELL STOP sits below it at stopPrice=95.
    matcher.placeOrder(makeLimit(1, BUY, 100, 1));
    matcher.placeOrder(makeStop(2, SELL, 95, 1)); // dormant: 95 < highestBid(100)

    EXPECT_EQ(0, matcher.notifier->matches.size());
    EXPECT_EQ(0, matcher.notifier->cancellations.size());

    // Market SELL #3 drains the only bid @100. While scanning downward, the
    // stop bin@95 is reached — stop #2 is activated. After the SELL market order
    // completes, the while loop fires stop #2 as a market sell, but there are
    // no bids left — stop #2 is cancelled.
    matcher.placeOrder(makeMarket(3, SELL, 1));
    EXPECT_EQ(1, matcher.notifier->matches.size());
    EXPECT_EQ(1, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);

    EXPECT_EQ(1,   matcher.notifier->matches[0].buyer.ordId);
    EXPECT_EQ(3,   matcher.notifier->matches[0].seller.ordId);
    EXPECT_EQ(1,   matcher.notifier->matches[0].qty);
    EXPECT_EQ(100, matcher.notifier->matches[0].price);

    // Stop #2 activated but found no bids — cancelled
    EXPECT_EQ(2, matcher.notifier->cancellations[0].ordId);

    // Market BUY #4 placed on an empty book — no asks exist, cancelled immediately
    matcher.placeOrder(makeMarket(4, BUY, 1));
    EXPECT_EQ(1, matcher.notifier->matches.size());
    EXPECT_EQ(2, matcher.notifier->cancellations.size());
    EXPECT_TRUE(matcher.getSpread().bidsMissing);
    EXPECT_TRUE(matcher.getSpread().asksMissing);

    EXPECT_EQ(4, matcher.notifier->cancellations[1].ordId);
}

TEST_F(MatcherTest, GetDepth_NoMatchReflectsAllLevels){
    // Place 3 bid levels and 3 ask levels with no overlap
    matcher.placeOrder(makeLimit(1, BUY,  90, 1));
    matcher.placeOrder(makeLimit(2, BUY,  95, 2));
    matcher.placeOrder(makeLimit(3, BUY, 100, 3));
    matcher.placeOrder(makeLimit(4, SELL, 110, 1));
    matcher.placeOrder(makeLimit(5, SELL, 115, 2));
    matcher.placeOrder(makeLimit(6, SELL, 120, 3));

    const Depth depth = matcher.getDepth();

    // Bids: highest price first
    ASSERT_EQ(3, depth.bidBins.size());
    EXPECT_EQ(100, depth.bidBins[0].price);
    EXPECT_EQ(3,   depth.bidBins[0].totalQty);
    EXPECT_EQ(95,  depth.bidBins[1].price);
    EXPECT_EQ(2,   depth.bidBins[1].totalQty);
    EXPECT_EQ(90,  depth.bidBins[2].price);
    EXPECT_EQ(1,   depth.bidBins[2].totalQty);

    // Asks: lowest price first
    ASSERT_EQ(3, depth.askBins.size());
    EXPECT_EQ(110, depth.askBins[0].price);
    EXPECT_EQ(1,   depth.askBins[0].totalQty);
    EXPECT_EQ(115, depth.askBins[1].price);
    EXPECT_EQ(2,   depth.askBins[1].totalQty);
    EXPECT_EQ(120, depth.askBins[2].price);
    EXPECT_EQ(3,   depth.askBins[2].totalQty);
}

TEST_F(MatcherTest, GetDepth_DrainedLevelIsExcluded){
    // Three ask levels; a market buy will fully consume the best ask (110)
    matcher.placeOrder(makeLimit(1, SELL, 110, 2));
    matcher.placeOrder(makeLimit(2, SELL, 115, 2));
    matcher.placeOrder(makeLimit(3, BUY,  100, 2));

    matcher.placeOrder(makeMarket(4, BUY, 2)); // drains ask@110 entirely

    const Depth depth = matcher.getDepth();

    // ask@110 is gone; only ask@115 remains
    ASSERT_EQ(1, depth.askBins.size());
    EXPECT_EQ(115, depth.askBins[0].price);
    EXPECT_EQ(2,   depth.askBins[0].totalQty);

    // bid@100 is still present
    ASSERT_EQ(1, depth.bidBins.size());
    EXPECT_EQ(100, depth.bidBins[0].price);
    EXPECT_EQ(2,   depth.bidBins[0].totalQty);
}