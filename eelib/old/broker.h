#pragma once

#include <vector>
#include <unordered_map>
#include <map>

#include "order.h"
#include "match.h"


// TODO add broker constrained flag to Agent?
// not all agents should have limited resources.
// maybe some Environment class can be a source/sink for resouces.

// Copied from agent.h
// This will probably be the replacement

struct AssetQty{
    long qty = 0;
    long qtyWithheld = 0;
};

// TODO should there be guards against negative values? fine for now
struct Inventory {
    std::map<std::string, AssetQty> assets{};
    long cashBalance = 0;
    long cashWithheld = 0;

    Inventory() = default;

    int netQty(const std::string& asset) const {
        auto it = assets.find(asset);
        return it == assets.end() ? 0 : (it->second.qty - it->second.qtyWithheld);
    }

    long netCash() const {
        return cashBalance - cashWithheld;
    }

    void withholdCash(long amount) {
        cashWithheld += amount;
    }

    void withholdAsset(const std::string& asset, long amount) {
        assets[asset].qtyWithheld += amount;
    }

    AssetQty& getAssetQty(const std::string& asset);
};

struct Account{
    long traderId = 0;
    Inventory inventory;
};

class Broker{
    // traderId - Account
    std::unordered_map<long, Account> accounts;

    const Inventory& observeInventory(long traderId) const;
    Inventory& getInventory(long traderId);

    public:
        bool canPlaceOrder(const Order& order) const;
        void withholdOrder(const Order& order);
        void openAccount(long traderId);
        void closeAccount(long traderId);
        void settleMatch(const Match& match);

        // TODO relsease witheld cancelled orders
        void releaseWithhold();
};