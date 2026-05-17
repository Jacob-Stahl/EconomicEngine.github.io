#include "broker.h"

const Inventory& Broker::observeInventory(long traderId) const {
    return accounts.at(traderId).inventory;
}

Inventory& Broker::getInventory(long traderId) {
    return accounts.at(traderId).inventory;
}

bool Broker::canPlaceOrder(const Order& order) const{
    auto& inventory = observeInventory(order.traderId);

    // There is an assumption here that the amount withheld is always == amount transfered when the trade is matched.

    // TODO: BUY MARKETS are tricky because the price is undefined.
    // Apparently this is not a trivial problem to solve
    
    if(order.side == BUY && order.type == LIMIT){
        if(inventory.netCash() >= (long)order.qty * (long)order.price){
            return true;
        }
    }

    if(order.side == BUY && order.type == STOPLIMIT){
        if(inventory.netCash() >= (long)order.qty * (long)order.stopPrice){
            return true;
        }
    }

    if(order.side == SELL){
        if(inventory.netQty(order.asset) >= order.qty){
            return true;
        }
    }

    return false;
}

void Broker::openAccount(long traderId) {
    accounts.emplace(traderId, Account{traderId});
}

void Broker::closeAccount(long traderId) {
    accounts.erase(traderId);
}

void Broker::withholdOrder(const Order& order) {
    auto& inventory = getInventory(order.traderId);

    if (order.side == BUY && order.type == LIMIT) {
        inventory.withholdCash((long)order.qty * order.price);
    } else if (order.side == BUY && order.type == STOPLIMIT) {
        inventory.withholdCash((long)order.qty * order.stopPrice);
    } else if (order.side == SELL) {
        inventory.withholdAsset(order.asset, order.qty);
    }
}

void Broker::settleMatch(const Match& match){
    auto& asset = match.buyer.asset; // buyer and seller will have the same asset

    auto& buyerInventory = getInventory(match.buyer.traderId);
    auto& sellerInventory = getInventory(match.seller.traderId);

    long cashTransfered = match.cashTransfered();
    long qtyTransfered = match.qty;

    // Transfer cash
    buyerInventory.cashBalance -= cashTransfered;
    buyerInventory.cashWithheld -= cashTransfered;
    sellerInventory.cashBalance += cashTransfered;

    // Transfer assets
    auto& buyerAssetQty = buyerInventory.getAssetQty(asset);
    auto& sellerAssetQty = sellerInventory.getAssetQty(asset);

    sellerAssetQty.qty -= qtyTransfered;
    sellerAssetQty.qtyWithheld -= qtyTransfered;
    buyerAssetQty.qty += qtyTransfered;
}