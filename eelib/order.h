#pragma once

#include <string>

struct Spread{
    bool bidsMissing = true;
    bool asksMissing = true;

    int highestBid = 0;
    int lowestAsk = 0;
};

/// @brief Subset of the order types found here: https://www.onixs.biz/fix-dictionary/4.4/tagNum_40.html
enum OrdType{

    /// @brief matched with the best limit on the book
    MARKET = 1,

    /// @brief buy or sell a specific price
    LIMIT = 2,

    /// @brief matched with the best limit on the book, above/below a desired price threshold
    STOP = 3,

    /// @brief matched with the best market on the book, above/below a desired price threshold
    STOPLIMIT = 4
};

enum Side {
    BUY = 1,
    SELL = 2,
};

enum TimeInForce{
    /// @brief Good Til Cancelled
    GTC,

    /// @brief Immediate or Cancelled
    IOC,

    /// @brief Fill or Kill
    FOC
};

struct Order{
    char asset[16];
    long traderId = -1;
    long ordId = -1;
    Side side;
    OrdType type;
    TimeInForce timeInForce;
    int price = 0;
    unsigned int qty = 0;
    int stopPrice = 0;
};

class OrderBuilder{
    Order order{};
    bool typeSet = false;
    bool assetSet = false;
    bool traderIdSet = false;
    bool ordIdSet = false;

    public:
        OrderBuilder& limit(Side side, int price, unsigned int qty);
        OrderBuilder& market(Side side, unsigned int qty);
        OrderBuilder& stop(Side side, int stopPrice, unsigned int qty);
        OrderBuilder& stopLimit(Side side, int price, int stopPrice, unsigned int qty);

        OrderBuilder& withAsset(const std::string& asset);
        OrderBuilder& withTraderId(long traderId);
        OrderBuilder& withOrdId(long ordId);     
        
        Order build();
};