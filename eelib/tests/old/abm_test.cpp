#include <gtest/gtest.h>
#include "../abm.h"
#include "../agent.h"
#include "../agent_manager.h"

#include <utility>
#include <algorithm>

class MockAgent : public Agent {
public:
    MockAgent(long id) : Agent(id) {}
    
    Action policy(const Observation& observation) override {
        return Action();
    }
};

class ABMTest : public ::testing::Test {
protected:
    ABM abm;
};

class CountingTickCallback : public TickCallback {
public:
    int callCount = 0;

    void callBackAction() override {
        ++callCount;
    }
};

TEST_F(ABMTest, AddAgentReturnsCorrectId) {
    auto agent = std::make_unique<MockAgent>(0); // Initial ID doesn't matter
    long id = abm.addAgent(std::move(agent));
    
    EXPECT_EQ(id, 1);
}

TEST_F(ABMTest, AddMultipleAgentsIncrementIds) {
    long id1 = abm.addAgent(std::make_unique<MockAgent>(0));
    long id2 = abm.addAgent(std::make_unique<MockAgent>(0));
    
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId) {
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 1
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 2
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 3
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 4
    
    EXPECT_EQ(abm.getNumAgents(), 4);
    
    // Remove agent 3
    std::vector<long> traderIdsToRemove{3};
    abm.removeAgents(traderIdsToRemove);
    
    EXPECT_EQ(abm.getNumAgents(), 3);
}

TEST_F(ABMTest, Remove2AgentsBasedOnId) {
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 1
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 2
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 3
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 4
    
    EXPECT_EQ(abm.getNumAgents(), 4);
    
    // Remove agent 2 and 3
    std::vector<long> traderIdsToRemove{3, 2};
    abm.removeAgents(traderIdsToRemove);
    
    EXPECT_EQ(abm.getNumAgents(), 2);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId_IdNotPresent_DoesntRemove) {
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 1
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 2
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 3
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 4
    
    EXPECT_EQ(abm.getNumAgents(), 4);
    
    // Try to remove 10. Is not there
    std::vector<long> traderIdsToRemove{10};
    abm.removeAgents(traderIdsToRemove);
    
    EXPECT_EQ(abm.getNumAgents(), 4);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId_TraderIdsEmpty) {
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 1
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 2
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 3
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 4
    
    EXPECT_EQ(abm.getNumAgents(), 4);
    
    std::vector<long> traderIdsToRemove{};
    abm.removeAgents(traderIdsToRemove);
    
    EXPECT_EQ(abm.getNumAgents(), 4);
}

TEST_F(ABMTest, AgentIdsAscendingAfterAdd) {
    abm.addAgent(std::make_unique<MockAgent>(0));
    abm.addAgent(std::make_unique<MockAgent>(0));
    abm.addAgent(std::make_unique<MockAgent>(0));

    auto ids = abm.getAgentIds();
    EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));
}

TEST_F(ABMTest, AgentIdsAscendingAfterRemove) {
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 1
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 2
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 3
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 4

    std::vector<long> toRemove{2, 3};
    abm.removeAgents(toRemove);

    auto ids = abm.getAgentIds();
    EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));
}

class MockProducerAgent : public Agent {
public:
    std::string asset;
    std::vector<Match> matches;
    MockProducerAgent(long id, std::string asset_ = "FOOD") : Agent(id), asset(asset_) {}
    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::SELL;
            o.qty = 1;
            o.asset = asset;
            o.type = OrdType::MARKET;
            return Action(o); 
        }
        return Action();
    }
    void matchFound(const Match& match, tick now) override {
        matches.push_back(match);
    }
};

class MockConsumerAgent : public Agent {
public:
    std::string asset;
    std::vector<Match> matches;
    MockConsumerAgent(long id, std::string asset_ = "FOOD") : Agent(id), asset(asset_) {}
    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::BUY;
            o.qty = 1;
            o.price = 100;
            o.asset = asset;
            o.type = OrdType::LIMIT;
            return Action(o);
        }
        return Action();
    }
    void matchFound(const Match& match, tick now) override {
        matches.push_back(match);
    }
};

class TrackingConsumer : public Consumer {
public:
    using Consumer::Consumer;

    TrackingConsumer(long traderId, std::string asset, unsigned short maxPrice,
        tick hungerDelay)
        : Consumer(traderId, makeState(asset, maxPrice, hungerDelay)) {}

    std::vector<Action> actions;
    int matchFoundCalls = 0;

private:
    static std::shared_ptr<ConsumerState> makeState(
        const std::string& asset,
        unsigned short maxPrice,
        tick hungerDelay) {
        auto state = std::make_shared<ConsumerState>();
        state->asset = asset;
        state->maxPrice = maxPrice;
        state->hungerDelay = hungerDelay;
        return state;
    }

public:

    Action policy(const Observation& obs) override {
        Action action = Consumer::policy(obs);
        actions.push_back(action);
        return action;
    }

    void orderPlaced(long orderId, const tick now) override {
        Consumer::orderPlaced(orderId, now);
    }

    void orderCanceled(long orderId, const tick now) override {
        Consumer::orderCanceled(orderId, now);
    }

    void matchFound(const Match& match, const tick now) override {
        ++matchFoundCalls;
        Consumer::matchFound(match, now);
    }
};

TEST(ProducerTest, SharedStateTracksQtyPerTick) {
    auto state = std::make_shared<ProducerState>();
    state->asset = "FOOD";
    state->preferedPrice = 100;

    Producer producer(1, state);

    Observation observation;
    Spread spread;
    spread.bidsMissing = false;
    spread.highestBid = 105;
    observation.assetObservations[state->asset].spread = spread;

    Action action = producer.policy(observation);

    ASSERT_EQ(action.ordersToPlace.size(), 1);
    EXPECT_TRUE(action.orderIdsToCancel.empty());
    EXPECT_EQ(action.ordersToPlace[0].asset, state->asset);
    EXPECT_EQ(action.ordersToPlace[0].side, SELL);
    EXPECT_EQ(action.ordersToPlace[0].type, MARKET);
    EXPECT_EQ(action.ordersToPlace[0].qty, 2u);
    EXPECT_EQ(state->qtyPerTick, 2u);
}

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

TEST_F(ABMTest, TickCallbacksRunAfterEveryStep) {
    auto callback = std::make_unique<CountingTickCallback>();
    CountingTickCallback* callbackPtr = callback.get();
    abm.addTickCallback(std::move(callback));

    abm.simStep();
    abm.simStep();

    EXPECT_EQ(callbackPtr->callCount, 2);
}

TEST(RecipeTest, ConstructorAcceptsReadableBraceInitialization) {
    Recipe recipe({{"OIL", 2}, {"LABOR", 1}}, {{"FUEL", 1}}, 15);

    EXPECT_EQ(recipe.inputs.at("OIL"), 2);
    EXPECT_EQ(recipe.inputs.at("LABOR"), 1);
    EXPECT_EQ(recipe.outputs.at("FUEL"), 1);
    EXPECT_EQ(recipe.cost, 15);
}

TEST(ConsumerManagerTest, StateChangesPropagateToManagedConsumersInABM) {
    auto abm = std::make_shared<ABM>();
    ConsumerManager manager(abm, "consumers", "FOOD");

    manager.changeHungerDelay(0, 0);
    manager.changeMaxPrice(10, 0);
    manager.changeNumAgents(1);

    abm->simStep();
    abm->simStep();

    Depth initialDepth = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(initialDepth.bidBins.size(), 1);
    EXPECT_EQ(initialDepth.bidBins[0].price, 1);
    EXPECT_EQ(initialDepth.bidBins[0].totalQty, 1);

    manager.changeHungerDelay(100, 0);
    abm->simStep();

    Depth afterHungerDelayChange = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(afterHungerDelayChange.bidBins.empty());

    manager.changeHungerDelay(0, 0);
    manager.changeMaxPrice(3, 0);
    abm->simStep();

    Depth cappedAtThree = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(cappedAtThree.bidBins.size(), 1);
    EXPECT_EQ(cappedAtThree.bidBins[0].price, 3);

    manager.changeMaxPrice(1, 0);
    abm->simStep();

    Depth cappedAtOne = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(cappedAtOne.bidBins.size(), 1);
    EXPECT_EQ(cappedAtOne.bidBins[0].price, 1);
}

TEST(ProducerManagerTest, StateChangesPropagateToManagedProducersInABM) {
    auto abm = std::make_shared<ABM>();
    ProducerManager manager(abm, "producers", "FOOD");

    manager.changePreferedPrice(200, 0);
    manager.changeNumAgents(1);
    abm->addAgent(std::make_unique<MockConsumerAgent>(0));

    abm->simStep();
    abm->simStep();

    Depth depthBeforeChange = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(depthBeforeChange.bidBins.size(), 1);
    EXPECT_EQ(depthBeforeChange.bidBins[0].price, 100);
    EXPECT_EQ(depthBeforeChange.bidBins[0].totalQty, 1);

    manager.changePreferedPrice(50, 0);
    abm->simStep();

    Depth depthAfterChange = abm->getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(depthAfterChange.bidBins.empty());
}

TEST_F(ABMTest, ProducerConsumerOneStep) {
    // 1 Producer, 3 Consumers
    abm.addAgent(std::make_unique<MockProducerAgent>(0));
    abm.addAgent(std::make_unique<MockConsumerAgent>(0));
    abm.addAgent(std::make_unique<MockConsumerAgent>(0));
    abm.addAgent(std::make_unique<MockConsumerAgent>(0));

    // Run one iteration of simStep()
    // This executes the orders.
    abm.simStep();

    auto obs = abm.getLatestObservation();
    
    // Check tick counter via observation time
    // First step: time 0. Second step: time 1.
    EXPECT_EQ(obs.time, tick(1));

    // Make sure the FOOD order book is in an expected state
    ASSERT_TRUE(obs.assetObservations.count("FOOD"));
    Depth depth = obs.assetObservations.at("FOOD").depth;

    // Producer (Market Sell 1) should match with one Consumer (Limit Buy 1).
    // 3 Consumers total -> 3 Bids.
    // 1 Bid matched -> 2 Bids left.
    // No Asks left (Producer filled).

    ASSERT_EQ(depth.bidBins.size(), 1);
    EXPECT_EQ(depth.bidBins[0].price, 100);
    EXPECT_EQ(depth.bidBins[0].totalQty, 2); // 2 remaining

    EXPECT_TRUE(depth.askBins.empty());
}

TEST_F(ABMTest, MultipleStepsIncrementTickCounter) {
    int numSteps = 10;
    for(int i = 0; i < numSteps; ++i) {
        abm.simStep();
        // simStep increments tickCounter and calls observe() at the end
        EXPECT_EQ(abm.getLatestObservation().time, tick(i + 1)); 
    }
}

TEST_F(ABMTest, MatchRoutingToAgents) {
    // 1 Producer, 1 Consumer
    auto producer = std::make_unique<MockProducerAgent>(0);
    auto consumer = std::make_unique<MockConsumerAgent>(0);

    // Keep raw pointers to check state later
    MockProducerAgent* pProducer = producer.get();
    MockConsumerAgent* pConsumer = consumer.get();

    abm.addAgent(std::move(producer));
    abm.addAgent(std::move(consumer));

    // Run one iteration of simStep()
    abm.simStep();

    // Verify matches
    ASSERT_EQ(pProducer->matches.size(), 1);
    ASSERT_EQ(pConsumer->matches.size(), 1);

    Match prodMatch = pProducer->matches[0];
    Match consMatch = pConsumer->matches[0];

    // Check quantities
    EXPECT_EQ(prodMatch.qty, 1);
    EXPECT_EQ(consMatch.qty, 1);

    // Check trader Ids identify who was who
    EXPECT_EQ(prodMatch.seller.traderId, pProducer->traderId);
    EXPECT_EQ(prodMatch.buyer.traderId, pConsumer->traderId);
    
    EXPECT_EQ(consMatch.seller.traderId, pProducer->traderId);
    EXPECT_EQ(consMatch.buyer.traderId, pConsumer->traderId);
}

TEST_F(ABMTest, MatchRoutingToCorrectConsumerWithThreeConsumers) {
    auto producer = std::make_unique<MockProducerAgent>(0);
    auto consumer1 = std::make_unique<MockConsumerAgent>(0);
    auto consumer2 = std::make_unique<MockConsumerAgent>(0);
    auto consumer3 = std::make_unique<MockConsumerAgent>(0);

    MockProducerAgent* pProducer = producer.get();
    MockConsumerAgent* pConsumer1 = consumer1.get();
    MockConsumerAgent* pConsumer2 = consumer2.get();
    MockConsumerAgent* pConsumer3 = consumer3.get();

    abm.addAgent(std::move(producer));
    abm.addAgent(std::move(consumer1));
    abm.addAgent(std::move(consumer2));
    abm.addAgent(std::move(consumer3));

    abm.simStep();

    ASSERT_EQ(pProducer->matches.size(), 1);
    ASSERT_EQ(pConsumer1->matches.size(), 1);
    EXPECT_TRUE(pConsumer2->matches.empty());
    EXPECT_TRUE(pConsumer3->matches.empty());

    Match producerMatch = pProducer->matches[0];
    Match consumer1Match = pConsumer1->matches[0];

    EXPECT_EQ(producerMatch.qty, 1);
    EXPECT_EQ(consumer1Match.qty, 1);

    EXPECT_EQ(producerMatch.seller.traderId, pProducer->traderId);
    EXPECT_EQ(producerMatch.buyer.traderId, pConsumer1->traderId);
    EXPECT_NE(producerMatch.buyer.traderId, pConsumer2->traderId);
    EXPECT_NE(producerMatch.buyer.traderId, pConsumer3->traderId);

    EXPECT_EQ(consumer1Match.seller.traderId, pProducer->traderId);
    EXPECT_EQ(consumer1Match.buyer.traderId, pConsumer1->traderId);
}

TEST_F(ABMTest, RealConsumerMatchFoundResetsHungerAfterFill) {
    auto consumer = std::make_unique<TrackingConsumer>(0, "FOOD", 20, tick(0));
    auto producer = std::make_unique<Producer>(0, "FOOD", 0);

    TrackingConsumer* pConsumer = consumer.get();

    abm.addAgent(std::move(consumer));
    abm.addAgent(std::move(producer));

    abm.simStep();
    abm.simStep();
    abm.simStep();

    ASSERT_EQ(pConsumer->matchFoundCalls, 1);

    abm.simStep();

    ASSERT_EQ(pConsumer->actions.size(), 4);
    EXPECT_TRUE(pConsumer->actions[3].orderIdsToCancel.empty());

    Depth depthAfterReset = abm.getLatestObservation().assetObservations.at("FOOD").depth;
    EXPECT_TRUE(depthAfterReset.bidBins.empty());

    abm.simStep();

    Depth depthAfterRecovery = abm.getLatestObservation().assetObservations.at("FOOD").depth;
    ASSERT_EQ(depthAfterRecovery.bidBins.size(), 1);
    EXPECT_EQ(depthAfterRecovery.bidBins[0].price, 1);
    EXPECT_EQ(depthAfterRecovery.bidBins[0].totalQty, 1);
}

class CancelingAgent : public Agent {
public:
    bool cancellationConfirmed = false;
    long orderToCancel = -1;

    CancelingAgent(long id) : Agent(id) {}

    Action policy(const Observation& obs) override {

        // Place an order at tick 0, cancel it at tick 1
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::SELL;
            o.qty = 1;
            o.asset = "FOOD";
            o.type = OrdType::LIMIT;
            o.price = 100;
            return Action(o); 
        }
        if(obs.time == tick(1)){
             return Action(orderToCancel); // Cancel the order placed at tick 0
        }
        return Action();
    }

    void orderPlaced(long orderId, tick now) override {
        orderToCancel = orderId; // Store the order ID to cancel later
    }

    void orderCanceled(long orderId, tick now) override {
        cancellationConfirmed = true;
        orderToCancel = orderId;
    }
};

TEST_F(ABMTest, CancellationRouting) {
    auto agent = std::make_unique<CancelingAgent>(0);
    CancelingAgent* pAgent = agent.get();
    abm.addAgent(std::move(agent));

    // Tick 0 -> 1: Place Order
    abm.simStep();

    // Verify order is on the book
    auto obs = abm.getLatestObservation();
    Depth depth = obs.assetObservations.at("FOOD").depth;
    ASSERT_EQ(depth.askBins.size(), 1);
    EXPECT_EQ(depth.askBins[0].totalQty, 1);

    // Tick 1 -> 2: Cancel Order
    abm.simStep();

    // Verify cancellation callback
    EXPECT_TRUE(pAgent->cancellationConfirmed);

    // Verify order is gone from book
    obs = abm.getLatestObservation();
    depth = obs.assetObservations.at("FOOD").depth;
    EXPECT_TRUE(depth.askBins.empty());
}

TEST_F(ABMTest, MultipleAssetsNoCrossTalk) {
    // Create 1 producer and 1 consumer for FOOD
    auto foodProducer = std::make_unique<MockProducerAgent>(0, "FOOD");
    auto foodConsumer = std::make_unique<MockConsumerAgent>(0, "FOOD");
    MockProducerAgent* pFoodProd = foodProducer.get();
    MockConsumerAgent* pFoodCons = foodConsumer.get();

    // Create 1 producer and 1 consumer for WATER
    auto waterProducer = std::make_unique<MockProducerAgent>(0, "WATER");
    auto waterConsumer = std::make_unique<MockConsumerAgent>(0, "WATER");
    MockProducerAgent* pWaterProd = waterProducer.get();
    MockConsumerAgent* pWaterCons = waterConsumer.get();

    abm.addAgent(std::move(foodProducer));
    abm.addAgent(std::move(foodConsumer));
    abm.addAgent(std::move(waterProducer));
    abm.addAgent(std::move(waterConsumer));

    abm.simStep();

    // Verify matches for FOOD agents
    ASSERT_EQ(pFoodProd->matches.size(), 1);
    ASSERT_EQ(pFoodCons->matches.size(), 1);

    // Verify matches for WATER agents
    ASSERT_EQ(pWaterProd->matches.size(), 1);
    ASSERT_EQ(pWaterCons->matches.size(), 1);

    // Check assets in match details
    EXPECT_EQ(pFoodProd->matches[0].buyer.asset, "FOOD");
    EXPECT_EQ(pFoodProd->matches[0].seller.asset, "FOOD");
    
    EXPECT_EQ(pWaterProd->matches[0].buyer.asset, "WATER");
    EXPECT_EQ(pWaterProd->matches[0].seller.asset, "WATER");

    // Verify correct partners (no cross-talk)
    EXPECT_EQ(pFoodProd->matches[0].buyer.traderId, pFoodCons->traderId);
    EXPECT_EQ(pFoodCons->matches[0].seller.traderId, pFoodProd->traderId);

    EXPECT_EQ(pWaterProd->matches[0].buyer.traderId, pWaterCons->traderId);
    EXPECT_EQ(pWaterCons->matches[0].seller.traderId, pWaterProd->traderId);
}

class TickSpyAgent : public Agent {
public:
    tick lastOrderPlacedTick;
    tick lastOrderCanceledTick;
    tick lastMatchFoundTick;
    bool orderPlacedCalled = false;
    bool orderCanceledCalled = false;
    bool matchFoundCalled = false;
    std::vector<long> canceledOrderIds;
    std::vector<long> placedOrderIds;

    Action nextAction;

    TickSpyAgent(long id) : Agent(id) {}

    Action policy(const Observation& obs) override {
        return nextAction;
    }

    void orderPlaced(long orderId, tick now) override {
        lastOrderPlacedTick = now;
        orderPlacedCalled = true;
        placedOrderIds.push_back(orderId);
    }

    void orderCanceled(long orderId, tick now) override {
        lastOrderCanceledTick = now;
        orderCanceledCalled = true;
        canceledOrderIds.push_back(orderId);
    }

    void matchFound(const Match& match, tick now) override {
        lastMatchFoundTick = now;
        matchFoundCalled = true;
    }
};

static Order makeTestOrder(
    const std::string& asset,
    Side side,
    OrdType type,
    unsigned short price,
    unsigned int qty) {
    return Order(asset, side, type, price, qty);
}

TEST_F(ABMTest, AgentReceivesCorrectTickOnEvents) {
    auto agentPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* agent = agentPtr.get();
    abm.addAgent(std::move(agentPtr));

    // Tick 0: Place Order
    Order order("ASSET", BUY, LIMIT, 100, 1);
    order.ordId = 123;
    agent->nextAction = Action(order);
    
    abm.simStep(); // Tick becomes 1
    
    EXPECT_TRUE(agent->orderPlacedCalled);
    EXPECT_EQ(agent->lastOrderPlacedTick.raw(), 0);

    // Tick 1: Cancel Order
    agent->nextAction = Action(order.ordId); // Cancel previous order
    agent->orderPlacedCalled = false; // Reset flags
    
    abm.simStep(); // Tick becomes 2
    
    EXPECT_TRUE(agent->orderCanceledCalled);
    EXPECT_EQ(agent->lastOrderCanceledTick.raw(), 1);

    // Tick 2: Place Another Order
    agent->orderPlacedCalled = false;
    order.ordId = 124;
    agent->nextAction = Action(order);

    abm.simStep(); // Tick becomes 3

    EXPECT_TRUE(agent->orderPlacedCalled);
    EXPECT_EQ(agent->lastOrderPlacedTick.raw(), 2);
}

TEST_F(ABMTest, AssetVolumesPerTickAggregatesMatchedQuantityByAsset) {
    auto foodSellerOne = std::make_unique<TickSpyAgent>(0);
    auto foodSellerTwo = std::make_unique<TickSpyAgent>(0);
    auto foodBuyerOne = std::make_unique<TickSpyAgent>(0);
    auto foodBuyerTwo = std::make_unique<TickSpyAgent>(0);
    auto waterSeller = std::make_unique<TickSpyAgent>(0);
    auto waterBuyer = std::make_unique<TickSpyAgent>(0);

    TickSpyAgent* foodSellerOnePtr = foodSellerOne.get();
    TickSpyAgent* foodSellerTwoPtr = foodSellerTwo.get();
    TickSpyAgent* foodBuyerOnePtr = foodBuyerOne.get();
    TickSpyAgent* foodBuyerTwoPtr = foodBuyerTwo.get();
    TickSpyAgent* waterSellerPtr = waterSeller.get();
    TickSpyAgent* waterBuyerPtr = waterBuyer.get();

    abm.addAgent(std::move(foodBuyerOne));
    abm.addAgent(std::move(foodBuyerTwo));
    abm.addAgent(std::move(foodSellerOne));
    abm.addAgent(std::move(foodSellerTwo));
    abm.addAgent(std::move(waterBuyer));
    abm.addAgent(std::move(waterSeller));

    foodBuyerOnePtr->nextAction = Action(makeTestOrder("FOOD", BUY, LIMIT, 100, 1));
    foodBuyerTwoPtr->nextAction = Action(makeTestOrder("FOOD", BUY, LIMIT, 100, 1));
    foodSellerOnePtr->nextAction = Action(makeTestOrder("FOOD", SELL, MARKET, 0, 1));
    foodSellerTwoPtr->nextAction = Action(makeTestOrder("FOOD", SELL, MARKET, 0, 1));
    waterBuyerPtr->nextAction = Action(makeTestOrder("WATER", BUY, LIMIT, 100, 3));
    waterSellerPtr->nextAction = Action(makeTestOrder("WATER", SELL, MARKET, 0, 3));

    abm.simStep();

    const auto& obs = abm.getLatestObservation();
    ASSERT_EQ(obs.time, tick(1));
    EXPECT_EQ(obs.assetObservations.at("FOOD").volumePerTick, 2u);
    EXPECT_EQ(obs.assetObservations.at("WATER").volumePerTick, 3u);
}

TEST_F(ABMTest, AssetVolumesPerTickClearsOnNextTickWithoutMatches) {
    auto seller = std::make_unique<TickSpyAgent>(0);
    auto buyer = std::make_unique<TickSpyAgent>(0);

    TickSpyAgent* sellerPtr = seller.get();
    TickSpyAgent* buyerPtr = buyer.get();

    abm.addAgent(std::move(seller));
    abm.addAgent(std::move(buyer));

    buyerPtr->nextAction = Action(makeTestOrder("FOOD", BUY, LIMIT, 100, 2));
    sellerPtr->nextAction = Action(makeTestOrder("FOOD", SELL, MARKET, 0, 2));

    abm.simStep();

    const auto& firstObs = abm.getLatestObservation();
    ASSERT_EQ(firstObs.assetObservations.at("FOOD").volumePerTick, 2u);

    sellerPtr->nextAction = Action();
    buyerPtr->nextAction = Action();

    abm.simStep();

    const auto& secondObs = abm.getLatestObservation();
    EXPECT_EQ(secondObs.assetObservations.at("FOOD").volumePerTick, 0u);
}

TEST_F(ABMTest, AgentReceivesCorrectTickOnMatch) {
    // Producer/Consumer scenario to trigger a match
    auto producerPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* producer = producerPtr.get();
    
    auto consumerPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* consumer = consumerPtr.get();

    abm.addAgent(std::move(producerPtr));
    abm.addAgent(std::move(consumerPtr));

    // Tick 0: Producer places SELL
    Order sellOrder("ASSET", SELL, LIMIT, 100, 1);
    sellOrder.ordId = 1;
    sellOrder.traderId = producer->traderId;
    producer->nextAction = Action(sellOrder);
    consumer->nextAction = Action(); // Do nothing
    
    abm.simStep(); // Tick 0 -> 1

    EXPECT_TRUE(producer->orderPlacedCalled);
    EXPECT_EQ(producer->lastOrderPlacedTick.raw(), 0);

    // Tick 1: Consumer places BUY -> MATCH
    producer->nextAction = Action();
    
    Order buyOrder("ASSET", BUY, MARKET, 0, 1);
    buyOrder.ordId = 2;
    buyOrder.traderId = consumer->traderId;
    consumer->nextAction = Action(buyOrder);

    abm.simStep(); // Tick 1 -> 2

    // Producer receives match
    EXPECT_TRUE(producer->matchFoundCalled);
    EXPECT_EQ(producer->lastMatchFoundTick.raw(), 1);

    // Consumer receives match
    EXPECT_TRUE(consumer->matchFoundCalled);
    EXPECT_EQ(consumer->lastMatchFoundTick.raw(), 1);
    
    // Check placed tick for consumer
    EXPECT_TRUE(consumer->orderPlacedCalled);
    EXPECT_EQ(consumer->lastOrderPlacedTick.raw(), 1);

    // Tick 2: Step Forward, verify tick increments
    producer->orderPlacedCalled = false;
    sellOrder.ordId = 3;
    producer->nextAction = Action(sellOrder);
    consumer->nextAction = Action();

    abm.simStep(); // Tick 2 -> 3

    EXPECT_TRUE(producer->orderPlacedCalled);
    EXPECT_EQ(producer->lastOrderPlacedTick.raw(), 2);
}

TEST_F(ABMTest, AggregateActionProcessesMultipleCancellationsAndPlacements) {
    auto agentPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* agent = agentPtr.get();
    abm.addAgent(std::move(agentPtr));

    Order firstOrder("ASSET", BUY, LIMIT, 100, 1);
    Order secondOrder("ASSET", BUY, LIMIT, 101, 1);

    Action initialAction;
    initialAction.addOrder(firstOrder);
    initialAction.addOrder(secondOrder);
    agent->nextAction = initialAction;

    abm.simStep();

    ASSERT_EQ(agent->placedOrderIds.size(), 2);

    auto obs = abm.getLatestObservation();
    ASSERT_TRUE(obs.assetObservations.count("ASSET"));
    Depth depth = obs.assetObservations.at("ASSET").depth;
    ASSERT_EQ(depth.bidBins.size(), 2);
    EXPECT_EQ(depth.bidBins[0].price, 101);
    EXPECT_EQ(depth.bidBins[1].price, 100);

    Action cancelAndReplace;
    cancelAndReplace.addCancellation(agent->placedOrderIds[0]);
    cancelAndReplace.addCancellation(agent->placedOrderIds[1]);

    Order replacementOne("ASSET", BUY, LIMIT, 99, 1);
    Order replacementTwo("ASSET", BUY, LIMIT, 98, 1);
    cancelAndReplace.addOrder(replacementOne);
    cancelAndReplace.addOrder(replacementTwo);
    agent->nextAction = cancelAndReplace;

    abm.simStep();

    ASSERT_EQ(agent->canceledOrderIds.size(), 2);
    EXPECT_EQ(agent->canceledOrderIds[0], agent->placedOrderIds[0]);
    EXPECT_EQ(agent->canceledOrderIds[1], agent->placedOrderIds[1]);
    ASSERT_EQ(agent->placedOrderIds.size(), 4);

    obs = abm.getLatestObservation();
    depth = obs.assetObservations.at("ASSET").depth;
    ASSERT_EQ(depth.bidBins.size(), 2);
    EXPECT_EQ(depth.bidBins[0].price, 99);
    EXPECT_EQ(depth.bidBins[0].totalQty, 1);
    EXPECT_EQ(depth.bidBins[1].price, 98);
    EXPECT_EQ(depth.bidBins[1].totalQty, 2);
}
