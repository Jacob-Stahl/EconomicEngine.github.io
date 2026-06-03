#include <gtest/gtest.h>
#include "../agents/abm.h"
#include "../agents/agent_manager.h"

#include <utility>
#include <algorithm>



// ---------------------------------------------------------------------------
// Mock agents
// ---------------------------------------------------------------------------

class MockAgent : public Agent {
public:
    MockAgent() : Agent() {}
    Action policy(const Observation&) override { return actionBuilder.build(); }
};

class MockProducerAgent : public Agent {
public:
    std::string asset;
    std::vector<Match> matches;
    MockProducerAgent(std::string asset_ = "FOOD") : Agent(), asset(std::move(asset_)) {}

    Action policy(const Observation& obs) override {
        if (obs.time == 0) {
            Order o = OrderBuilder()
                .limit(SELL, 100, 1)
                .withAsset(asset)
                .withTraderId(traderId)
                .withIncrementedOrderId()
                .build();
            return actionBuilder.withOrder(o).build();
        }
        return actionBuilder.build();
    }

    void matchFound(const Match& match, tick now) override { matches.push_back(match); }
};

class MockConsumerAgent : public Agent {
public:
    std::string asset;
    std::vector<Match> matches;
    MockConsumerAgent(std::string asset_ = "FOOD") : Agent(), asset(std::move(asset_)) {}

    Action policy(const Observation& obs) override {
        if (obs.time == 0) {
            Order o = OrderBuilder()
                .limit(BUY, 100, 1)
                .withAsset(asset)
                .withTraderId(traderId)
                .withIncrementedOrderId()
                .build();
            return actionBuilder.withOrder(o).build();
        }
        return actionBuilder.build();
    }

    void matchFound(const Match& match, tick now) override { matches.push_back(match); }
};

class TrackingConsumer : public Consumer {
    static std::shared_ptr<ConsumerState> makeState(
        const std::string& asset, std::int32_t maxPrice, tick hungerDelay)
    {
        auto s = std::make_shared<ConsumerState>();
        s->asset = asset;
        s->maxPrice = maxPrice;
        s->hungerDelay = hungerDelay;
        return s;
    }

public:
    std::vector<Action> actions;
    int matchFoundCalls = 0;

    TrackingConsumer(std::string asset, std::int32_t maxPrice, tick hungerDelay)
        : Consumer(makeState(asset, maxPrice, hungerDelay))
    {}

    Action policy(const Observation& obs) override {
        Action action = Consumer::policy(obs);
        actions.push_back(action);
        return action;
    }

    void orderPlaced(std::int64_t orderId, const tick now) override { Consumer::orderPlaced(orderId, now); }
    void orderCanceled(std::int64_t orderId, const tick now) override { Consumer::orderCanceled(orderId, now); }
    void matchFound(const Match& match, const tick now) override {
        ++matchFoundCalls;
        Consumer::matchFound(match, now);
    }
};

class CancelingAgent : public Agent {
public:
    bool cancellationConfirmed = false;
    std::int64_t orderToCancel = -1;

    CancelingAgent() : Agent() {}

    Action policy(const Observation& obs) override {
        if (obs.time == 0) {
            Order o = OrderBuilder()
                .limit(SELL, 100, 1)
                .withAsset("FOOD")
                .withTraderId(traderId)
                .withIncrementedOrderId()
                .build();
            return actionBuilder.withOrder(o).build();
        }
        if (obs.time == 1) {
            return actionBuilder.withCancellation(orderToCancel).build();
        }
        return actionBuilder.build();
    }

    void orderPlaced(std::int64_t orderId, tick now) override { orderToCancel = orderId; }
    void orderCanceled(std::int64_t orderId, tick now) override {
        cancellationConfirmed = true;
        orderToCancel = orderId;
    }
};

// TickSpyAgent stores next orders/cancellations as plain vectors (Action's
// constructor is private; we rebuild the Action inside policy()).

static Order makeTestOrder(
    const std::string& asset,
    Side side,
    OrdType type,
    std::int32_t price,
    std::uint32_t qty)
{
    OrderBuilder ob;
    switch (type) {
        case LIMIT:  return ob.limit(side, price, qty).withAsset(asset).withTraderId(0).withIncrementedOrderId().build();
        default:     return ob.market(side, qty).withAsset(asset).withTraderId(0).withIncrementedOrderId().build();
    }
}

class TickSpyAgent : public Agent {
public:
    tick lastOrderPlacedTick = 0;
    tick lastOrderCanceledTick = 0;
    tick lastMatchFoundTick = 0;
    bool orderPlacedCalled = false;
    bool orderCanceledCalled = false;
    bool matchFoundCalled = false;
    std::vector<std::int64_t> canceledOrderIds;
    std::vector<std::int64_t> placedOrderIds;

    std::vector<Order> nextOrders;
    std::vector<std::int64_t> nextCancellations;

    TickSpyAgent() : Agent() {}

    Action policy(const Observation&) override {

        // Ensure order trader id is consistant with the agent
        for(auto& order : nextOrders){
            order.traderId = traderId;
        }

        return actionBuilder
            .withOrders(nextOrders)
            .withCancellations(nextCancellations)
            .build();
    }

    void orderPlaced(std::int64_t orderId, tick now) override {
        lastOrderPlacedTick = now;
        orderPlacedCalled = true;
        placedOrderIds.push_back(orderId);
    }
    void orderCanceled(std::int64_t orderId, tick now) override {
        lastOrderCanceledTick = now;
        orderCanceledCalled = true;
        canceledOrderIds.push_back(orderId);
    }
    void matchFound(const Match& match, tick now) override {
        lastMatchFoundTick = now;
        matchFoundCalled = true;
    }
};

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

class ABMTest : public ::testing::Test {
protected:
    ABM abm;

    ABMTest(){
        // Resets incrementing trader Ids for each test
        // TODO something like: Agent::nextTraderId = 0;
    }
};

class CountingTickCallback : public TickCallback {
public:
    int callCount = 0;
    void callBackAction() override { ++callCount; }
};

// ---------------------------------------------------------------------------
// ABM agent management
// ---------------------------------------------------------------------------

TEST_F(ABMTest, AddAgent) {
    abm.addAgent<MockAgent>();
}

TEST_F(ABMTest, AddMultipleAgentsIncrementIds) {
    std::int64_t id1 = abm.addAgent<MockAgent>().traderId;
    std::int64_t id2 = abm.addAgent<MockAgent>().traderId;
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId) {
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    EXPECT_EQ(abm.getNumAgents(), 4u);

    std::vector<std::int64_t> toRemove{3};
    abm.removeAgents(toRemove);
    EXPECT_EQ(abm.getNumAgents(), 3u);
}

TEST_F(ABMTest, Remove2AgentsBasedOnId) {
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    EXPECT_EQ(abm.getNumAgents(), 4u);

    std::vector<std::int64_t> toRemove{3, 2};
    abm.removeAgents(toRemove);
    EXPECT_EQ(abm.getNumAgents(), 2u);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId_IdNotPresent_DoesntRemove) {
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    EXPECT_EQ(abm.getNumAgents(), 4u);

    std::vector<std::int64_t> toRemove{10};
    abm.removeAgents(toRemove);
    EXPECT_EQ(abm.getNumAgents(), 4u);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId_TraderIdsEmpty) {
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    EXPECT_EQ(abm.getNumAgents(), 4u);

    std::vector<std::int64_t> toRemove{};
    abm.removeAgents(toRemove);
    EXPECT_EQ(abm.getNumAgents(), 4u);
}

TEST_F(ABMTest, AgentIdsAscendingAfterAdd) {
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();

    auto ids = abm.getAgentIds();
    EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));
}

TEST_F(ABMTest, AgentIdsAscendingAfterRemove) {
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();
    abm.addAgent<MockAgent>();

    std::vector<std::int64_t> toRemove{2, 3};
    abm.removeAgents(toRemove);

    auto ids = abm.getAgentIds();
    EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));
}

// ---------------------------------------------------------------------------
// Tick callbacks
// ---------------------------------------------------------------------------

TEST_F(ABMTest, TickCallbacksRunAfterEveryStep) {
    auto callback = std::make_unique<CountingTickCallback>();
    CountingTickCallback* ptr = callback.get();
    abm.addTickCallback(std::move(callback));

    abm.simStep();
    abm.simStep();

    EXPECT_EQ(ptr->callCount, 2);
}

// ---------------------------------------------------------------------------
// Matching
// ---------------------------------------------------------------------------

TEST_F(ABMTest, ProducerConsumerOneStep) {
    abm.addAgent<MockProducerAgent>();
    abm.addAgent<MockConsumerAgent>();
    abm.addAgent<MockConsumerAgent>();
    abm.addAgent<MockConsumerAgent>();

    abm.simStep();

    auto obs = abm.getLatestObservation();
    EXPECT_EQ(obs.time, tick(1));

    ASSERT_TRUE(obs.assetObservations.count("FOOD"));
    Depth depth = obs.assetObservations.at("FOOD").depth;

    ASSERT_EQ(depth.bidBins.size(), 1u);
    EXPECT_EQ(depth.bidBins[0].price, 100);
    EXPECT_EQ(depth.bidBins[0].totalQty, 2u);
    EXPECT_TRUE(depth.askBins.empty());
}

TEST_F(ABMTest, MultipleStepsIncrementTickCounter) {
    int numSteps = 10;
    for (int i = 0; i < numSteps; ++i) {
        abm.simStep();
        EXPECT_EQ(abm.getLatestObservation().time, tick(i + 1));
    }
}

TEST_F(ABMTest, MatchRoutingToAgents) {
    MockProducerAgent& pProd = abm.addAgent<MockProducerAgent>();
    MockConsumerAgent& pCons = abm.addAgent<MockConsumerAgent>();
    abm.simStep();

    ASSERT_EQ(pProd.matches.size(), 1u);
    ASSERT_EQ(pCons.matches.size(), 1u);

    EXPECT_EQ(pProd.matches[0].qty, 1u);
    EXPECT_EQ(pCons.matches[0].qty, 1u);
    EXPECT_EQ(pProd.matches[0].seller.traderId, pProd.traderId);
    EXPECT_EQ(pProd.matches[0].buyer.traderId, pCons.traderId);
    EXPECT_EQ(pCons.matches[0].seller.traderId, pProd.traderId);
    EXPECT_EQ(pCons.matches[0].buyer.traderId, pCons.traderId);
}

TEST_F(ABMTest, MatchRoutingToCorrectConsumerWithThreeConsumers) {
    MockProducerAgent& pProd = abm.addAgent<MockProducerAgent>();
    MockConsumerAgent& pC1   = abm.addAgent<MockConsumerAgent>();
    MockConsumerAgent& pC2   = abm.addAgent<MockConsumerAgent>();
    MockConsumerAgent& pC3   = abm.addAgent<MockConsumerAgent>();
    abm.simStep();

    ASSERT_EQ(pProd.matches.size(), 1u);
    ASSERT_EQ(pC1.matches.size(), 1u);
    EXPECT_TRUE(pC2.matches.empty());
    EXPECT_TRUE(pC3.matches.empty());

    EXPECT_EQ(pProd.matches[0].seller.traderId, pProd.traderId);
    EXPECT_EQ(pProd.matches[0].buyer.traderId, pC1.traderId);
    EXPECT_EQ(pC1.matches[0].seller.traderId, pProd.traderId);
    EXPECT_EQ(pC1.matches[0].buyer.traderId, pC1.traderId);
}

TEST_F(ABMTest, CancellationRouting) {
    CancelingAgent& pAgent = abm.addAgent<CancelingAgent>();

    abm.simStep(); // tick 0->1: place order

    Depth depth = abm.getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(depth.askBins.size(), 1u);
    EXPECT_EQ(depth.askBins[0].totalQty, 1u);

    abm.simStep(); // tick 1->2: cancel order

    EXPECT_TRUE(pAgent.cancellationConfirmed);
    depth = abm.getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(depth.askBins.empty());
}

TEST_F(ABMTest, MultipleAssetsNoCrossTalk) {
    MockProducerAgent& pFP = abm.addAgent<MockProducerAgent>("FOOD");
    MockConsumerAgent& pFC = abm.addAgent<MockConsumerAgent>("FOOD");
    MockProducerAgent& pWP = abm.addAgent<MockProducerAgent>("WATER");
    MockConsumerAgent& pWC = abm.addAgent<MockConsumerAgent>("WATER");
    abm.simStep();

    // Check Agent States
    ASSERT_EQ(pFP.matches.size(), 1u);
    ASSERT_EQ(pFC.matches.size(), 1u);
    ASSERT_EQ(pWP.matches.size(), 1u);
    ASSERT_EQ(pWC.matches.size(), 1u);

    EXPECT_EQ(std::string(pFP.matches[0].buyer.asset), "FOOD");
    EXPECT_EQ(std::string(pFC.matches[0].seller.asset), "FOOD");
    EXPECT_EQ(std::string(pWP.matches[0].buyer.asset), "WATER");
    EXPECT_EQ(std::string(pWC.matches[0].seller.asset), "WATER");

    EXPECT_EQ(pFP.matches[0].buyer.traderId, pFC.traderId);
    EXPECT_EQ(pFC.matches[0].seller.traderId, pFP.traderId);
    EXPECT_EQ(pWP.matches[0].buyer.traderId, pWC.traderId);
    EXPECT_EQ(pWC.matches[0].seller.traderId, pWP.traderId);
}

// ---------------------------------------------------------------------------
// TickSpyAgent — callback timing
// ---------------------------------------------------------------------------

TEST_F(ABMTest, AgentReceivesCorrectTickOnEvents) {
    TickSpyAgent& agent = abm.addAgent<TickSpyAgent>();

    // Tick 0: place order
    agent.nextOrders = {makeTestOrder("ASSET", BUY, LIMIT, 100, 1)};
    abm.simStep(); // tick 0->1

    EXPECT_TRUE(agent.orderPlacedCalled);
    EXPECT_EQ(agent.lastOrderPlacedTick, tick(1));

    // Tick 1: cancel that order
    agent.nextOrders = {};
    agent.nextCancellations = {agent.placedOrderIds.back()};
    agent.orderPlacedCalled = false;
    abm.simStep(); // tick 1->2

    EXPECT_TRUE(agent.orderCanceledCalled);
    EXPECT_EQ(agent.lastOrderCanceledTick, tick(2));

    // Tick 2: place another order
    agent.orderPlacedCalled = false;
    agent.nextOrders = {makeTestOrder("ASSET", BUY, LIMIT, 100, 1)};
    agent.nextCancellations = {};
    abm.simStep(); // tick 2->3

    EXPECT_TRUE(agent.orderPlacedCalled);
    EXPECT_EQ(agent.lastOrderPlacedTick, tick(3));
}

TEST_F(ABMTest, AgentReceivesCorrectTickOnMatch) {
    TickSpyAgent& producer = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& consumer = abm.addAgent<TickSpyAgent>();

    // Tick 0: producer places SELL LIMIT 100
    producer.nextOrders = {makeTestOrder("ASSET", SELL, LIMIT, 100, 1)};
    consumer.nextOrders = {};
    consumer.nextCancellations = {};
    abm.simStep(); // tick 0->1

    EXPECT_TRUE(producer.orderPlacedCalled);
    EXPECT_EQ(producer.lastOrderPlacedTick, tick(1));

    // Tick 1: consumer places BUY LIMIT 100 -> match
    producer.nextOrders = {};
    producer.nextCancellations = {};
    consumer.nextOrders = {makeTestOrder("ASSET", BUY, LIMIT, 100, 1)};
    abm.simStep(); // tick 1->2


    // Check orders placed
    EXPECT_TRUE(consumer.orderPlacedCalled);
    EXPECT_EQ(consumer.lastOrderPlacedTick, tick(2));

    // Check matches
    EXPECT_TRUE(producer.matchFoundCalled);
    EXPECT_EQ(producer.lastMatchFoundTick, tick(2));
    EXPECT_TRUE(consumer.matchFoundCalled);
    EXPECT_EQ(consumer.lastMatchFoundTick, tick(2));
    EXPECT_TRUE(consumer.orderPlacedCalled);
    EXPECT_EQ(consumer.lastOrderPlacedTick, tick(2));

    // Tick 2: producer places another sell
    producer.orderPlacedCalled = false;
    producer.nextOrders = {makeTestOrder("ASSET", SELL, LIMIT, 100, 1)};
    consumer.nextOrders = {};
    consumer.nextCancellations = {};
    abm.simStep(); // tick 2->3

    EXPECT_TRUE(producer.orderPlacedCalled);
    EXPECT_EQ(producer.lastOrderPlacedTick, tick(3));
}

TEST_F(ABMTest, AggregateActionProcessesMultipleCancellationsAndPlacements) {
    TickSpyAgent& agent = abm.addAgent<TickSpyAgent>();

    // Step 1: place two limit buy orders
    agent.nextOrders = {
        makeTestOrder("ASSET", BUY, LIMIT, 100, 1),
        makeTestOrder("ASSET", BUY, LIMIT, 101, 1)
    };
    abm.simStep();

    ASSERT_EQ(agent.placedOrderIds.size(), 2u);
    Depth depth = abm.getLatestObservation().assetObservations.at("ASSET").depth;
    ASSERT_EQ(depth.bidBins.size(), 2u);
    EXPECT_EQ(depth.bidBins[0].price, 101);
    EXPECT_EQ(depth.bidBins[1].price, 100);

    // Step 2: cancel both, place two replacement orders
    agent.nextCancellations = {agent.placedOrderIds[0], agent.placedOrderIds[1]};
    agent.nextOrders = {
        makeTestOrder("ASSET", BUY, LIMIT, 99, 1),
        makeTestOrder("ASSET", BUY, LIMIT, 98, 1)
    };
    abm.simStep();

    ASSERT_EQ(agent.canceledOrderIds.size(), 2u);
    EXPECT_EQ(agent.canceledOrderIds[0], agent.placedOrderIds[0]);
    EXPECT_EQ(agent.canceledOrderIds[1], agent.placedOrderIds[1]);
    ASSERT_EQ(agent.placedOrderIds.size(), 4u);

    depth = abm.getLatestObservation().assetObservations.at("ASSET").depth;
    ASSERT_EQ(depth.bidBins.size(), 2u);
    EXPECT_EQ(depth.bidBins[0].price, 99);
    EXPECT_EQ(depth.bidBins[0].totalQty, 1u);
    EXPECT_EQ(depth.bidBins[1].price, 98);
    EXPECT_EQ(depth.bidBins[1].totalQty, 1u);
}

// ---------------------------------------------------------------------------
// Asset volume tracking
// ---------------------------------------------------------------------------

TEST_F(ABMTest, AssetVolumesPerTickAggregatesMatchedQuantityByAsset) {
    TickSpyAgent& fB1 = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& fB2 = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& fS1 = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& fS2 = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& wB  = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& wS  = abm.addAgent<TickSpyAgent>();

    fB1.nextOrders = {makeTestOrder("FOOD", BUY, LIMIT, 100, 1)};
    fB2.nextOrders = {makeTestOrder("FOOD", BUY, LIMIT, 100, 1)};
    fS1.nextOrders = {makeTestOrder("FOOD", SELL, MARKET, 0, 1)};
    fS2.nextOrders = {makeTestOrder("FOOD", SELL, MARKET, 0, 1)};
    wB.nextOrders  = {makeTestOrder("WATER", BUY, LIMIT, 100, 3)};
    wS.nextOrders  = {makeTestOrder("WATER", SELL, MARKET, 0, 3)};

    abm.simStep();

    const auto& obs = abm.getLatestObservation();
    ASSERT_EQ(obs.time, tick(1));
    EXPECT_EQ(obs.assetObservations.at("FOOD").volumePerTick, 2u);
    EXPECT_EQ(obs.assetObservations.at("WATER").volumePerTick, 3u);
}

TEST_F(ABMTest, AssetVolumesPerTickClearsOnNextTickWithoutMatches) {
    TickSpyAgent& pSell = abm.addAgent<TickSpyAgent>();
    TickSpyAgent& pBuy  = abm.addAgent<TickSpyAgent>();

    pBuy.nextOrders  = {makeTestOrder("FOOD", BUY, LIMIT, 100, 2)};
    pSell.nextOrders = {makeTestOrder("FOOD", SELL, LIMIT, 100, 2)};
    abm.simStep();

    ASSERT_EQ(abm.getLatestObservation().assetObservations.at("FOOD").volumePerTick, 2u);

    pSell.nextOrders = {};
    pBuy.nextOrders  = {};
    abm.simStep();

    EXPECT_EQ(abm.getLatestObservation().assetObservations.at("FOOD").volumePerTick, 0u);
}

// ---------------------------------------------------------------------------
// Producer unit test
// ---------------------------------------------------------------------------

TEST(ProducerTest, SharedStateTracksQtyPerTick) {
    auto state = std::make_shared<ProducerState>();
    state->asset = "FOOD";
    state->preferedPrice = 100;
    // qtyPerTick starts at 0; one policy call with highestBid > preferedPrice increments to 1

    Producer producer(state);

    Observation obs;
    Spread spread;
    spread.bidsMissing = false;
    spread.highestBid = 105;
    obs.assetObservations[state->asset].spread = spread;

    Action action = producer.policy(obs);

    ASSERT_EQ(action.ordersToPlace.size(), 1u);
    EXPECT_TRUE(action.orderIdsToCancel.empty());
    EXPECT_EQ(action.ordersToPlace[0].asset, state->asset);
    EXPECT_EQ(action.ordersToPlace[0].side, SELL);
    EXPECT_EQ(action.ordersToPlace[0].type, MARKET);
    EXPECT_EQ(action.ordersToPlace[0].qty, 1u);
    EXPECT_EQ(state->qtyPerTick, 1u);
}

// ---------------------------------------------------------------------------
// Recipe
// ---------------------------------------------------------------------------

TEST(RecipeTest, ConstructorAcceptsReadableBraceInitialization) {
    Recipe recipe({{"OIL", 2}, {"LABOR", 1}}, {{"FUEL", 1}}, 15);

    EXPECT_EQ(recipe.inputs.at("OIL"), 2u);
    EXPECT_EQ(recipe.inputs.at("LABOR"), 1u);
    EXPECT_EQ(recipe.outputs.at("FUEL"), 1u);
    EXPECT_EQ(recipe.cost, 15);
}

// ---------------------------------------------------------------------------
// Manager tests
// ---------------------------------------------------------------------------

TEST(ProducerManagerTest, ChangeNumAgentsTracksABMSize) {
    auto abm = std::make_shared<ABM>();
    {
        ProducerManager manager(abm, "producers", "FOOD");
        manager.changePreferedPrice(100, 0);

        manager.changeNumAgents(3);
        EXPECT_EQ(abm->getNumAgents(), 3u);

        manager.changeNumAgents(1);
        EXPECT_EQ(abm->getNumAgents(), 1u);
    }
    EXPECT_EQ(abm->getNumAgents(), 0u);
}

TEST(ConsumerManagerTest, StateChangesPropagateToManagedConsumersInABM) {
    
    // Start with 1 agent with 0 hunger delay
    auto abm = std::make_shared<ABM>();
    ConsumerManager manager(abm, "consumers", "FOOD");
    manager.changeHungerDelay(0, 0);
    manager.changeMaxPrice(10, 0);
    manager.changeNumAgents(1);

    // BUY LIMIT placed
    abm->simStep();

    // First order cancelled, new order placed with a higher bid
    abm->simStep();

    // We expect 1 order on the book
    Depth initialDepth = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(initialDepth.bidBins.size(), 1u);
    EXPECT_EQ(initialDepth.bidBins[0].price, 1);
    EXPECT_EQ(initialDepth.bidBins[0].totalQty, 1u);

    // Apply a long hunger delay
    manager.changeHungerDelay(100, 0);
    abm->simStep();

    Depth afterHungerDelayChange = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(afterHungerDelayChange.bidBins.empty());

    manager.changeHungerDelay(0, 0);
    manager.changeMaxPrice(3, 0);
    abm->simStep();

    Depth cappedAtThree = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(cappedAtThree.bidBins.size(), 1u);
    EXPECT_EQ(cappedAtThree.bidBins[0].price, 3);

    manager.changeMaxPrice(1, 0);
    abm->simStep();

    Depth cappedAtOne = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(cappedAtOne.bidBins.size(), 1u);
    EXPECT_EQ(cappedAtOne.bidBins[0].price, 1);
}

TEST(ProducerManagerTest, StateChangesPropagateToManagedProducersInABM) {
    auto abm = std::make_shared<ABM>();
    ProducerManager manager(abm, "producers", "FOOD");

    manager.changePreferedPrice(200, 0);
    manager.changeNumAgents(1);
    abm->addAgent<MockConsumerAgent>();

    abm->simStep();
    abm->simStep();

    Depth depthBefore = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(depthBefore.bidBins.size(), 1u);
    EXPECT_EQ(depthBefore.bidBins[0].price, 100);
    EXPECT_EQ(depthBefore.bidBins[0].totalQty, 1u);

    manager.changePreferedPrice(50, 0);
    abm->simStep();

    Depth depthAfter = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(depthAfter.bidBins.empty());
}

TEST_F(ABMTest, RealConsumerMatchFoundResetsHungerAfterFill) {
    auto producerState = std::make_shared<ProducerState>();
    producerState->asset = "FOOD";
    producerState->preferedPrice = 0;
    producerState->qtyPerTick = 1;
    abm.addAgent<Producer>(producerState);

    TrackingConsumer& pConsumer = abm.addAgent<TrackingConsumer>("FOOD", 20, 0);

    abm.simStep();
    abm.simStep();
    abm.simStep();

    ASSERT_EQ(pConsumer.matchFoundCalls, 1);

    abm.simStep();

    ASSERT_EQ(pConsumer.actions.size(), 4u);
    EXPECT_TRUE(pConsumer.actions[3].orderIdsToCancel.empty());

    Depth depthAfterReset = abm.getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(depthAfterReset.bidBins.empty());

    abm.simStep(); // Hunger is 0, no order
    abm.simStep(); // Hunger is 1, order is placed

    Depth depthAfterRecovery = abm.getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(depthAfterRecovery.bidBins.size(), 1u);
    EXPECT_EQ(depthAfterRecovery.bidBins[0].price, 1);
    EXPECT_EQ(depthAfterRecovery.bidBins[0].totalQty, 1u);
}
