#include <emscripten/bind.h>
#include "order.h"
#include "matcher.h"
#include "agents/agent.h"
#include "agents/agent_manager.h"
#include "agents/abm.h"

using namespace emscripten;

std::shared_ptr<ABM> borrowed_abm(ABM* abm) {
    return std::shared_ptr<ABM>(abm, [](ABM*) {});
}

ConsumerManager* create_consumer_manager(ABM* abm, const std::string& name,
    const std::string& asset) {
    return new ConsumerManager(borrowed_abm(abm), name, asset);
}

ProducerManager* create_producer_manager(ABM* abm, const std::string& name,
    const std::string& asset) {
    return new ProducerManager(borrowed_abm(abm), name, asset);
}

EMSCRIPTEN_BINDINGS(eelib_module) {
    enum_<OrdType>("OrdType")
        .value("MARKET", OrdType::MARKET)
        .value("LIMIT", OrdType::LIMIT)
        .value("STOP", OrdType::STOP)
        .value("STOPLIMIT", OrdType::STOPLIMIT);

    enum_<Side>("Side")
        .value("BUY", Side::BUY)
        .value("SELL", Side::SELL);

    class_<tick>("tick");
    
    value_object<Spread>("Spread")
        .field("bidsMissing", &Spread::bidsMissing)
        .field("asksMissing", &Spread::asksMissing)
        .field("highestBid", &Spread::highestBid)
        .field("lowestAsk", &Spread::lowestAsk);

    value_object<PriceBin>("PriceBin")
        .field("price", &PriceBin::price)
        .field("totalQty", &PriceBin::totalQty);

    register_vector<std::string>("VectorString");
    register_vector<PriceBin>("VectorPriceBin");

    value_object<Depth>("Depth")
        .field("bidBins", &Depth::bidBins)
        .field("askBins", &Depth::askBins);


    value_object<AssetObservation>("AssetObservation")
        .field("spread", &AssetObservation::spread)
        .field("depth", &AssetObservation::depth)
        //.field("marketBacklog", &AssetObservation::marketBacklog)
        .field("volumePerTick", &AssetObservation::volumePerTick);

    register_map<std::string, AssetObservation>("MapStringAssetObservation");

    value_object<Observation>("Observation")
        .field("time", &Observation::time)
        .field("assetObservations", &Observation::assetObservations);

    class_<ConsumerManager>("ConsumerManager")
        .function("changeHungerDelay", &ConsumerManager::changeHungerDelay)
        .function("changeMaxPrice", &ConsumerManager::changeMaxPrice)
        .function("changeNumAgents", &ConsumerManager::changeNumAgents);

    class_<ProducerManager>("ProducerManager")
        .function("changePreferedPrice", &ProducerManager::changePreferedPrice)
        .function("changeNumAgents", &ProducerManager::changeNumAgents);

    function("createConsumerManager", &create_consumer_manager, allow_raw_pointers());
    function("createProducerManager", &create_producer_manager, allow_raw_pointers());

    class_<ABM>("ABM")
        .constructor<>()
        .function("simStep", &ABM::simStep)
        .function("getNumAgents", &ABM::getNumAgents)
        .function("getLatestObservation", &ABM::getLatestObservation);
}
