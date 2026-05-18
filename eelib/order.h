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
    std::int64_t traderId = -1;
    std::int64_t ordId = -1;
    Side side;
    OrdType type;
    TimeInForce timeInForce;
    std::int32_t price = 0;
    std::uint32_t qty = 0;
    std::int32_t stopPrice = 0;

    private:
        Order() = default;

    friend class OrderBuilder;

    // TODO remove these later
    friend class Matcher;
    friend class Notifier;

};

class OrderBuilder{
    Order order{};
    bool typeSet = false;
    bool assetSet = false;
    bool traderIdSet = false;
    bool ordIdSet = false;

    public:
        OrderBuilder& limit(Side side, std::int32_t price, std::uint32_t qty);
        OrderBuilder& market(Side side, std::uint32_t qty);
        OrderBuilder& stop(Side side, std::int32_t stopPrice, std::uint32_t qty);
        OrderBuilder& stopLimit(Side side, std::int32_t price, std::int32_t stopPrice, std::uint32_t qty);

        OrderBuilder& withAsset(const std::string& asset);
        OrderBuilder& withTraderId(std::int64_t traderId);
        OrderBuilder& withOrdId(std::int64_t ordId);     
        
        Order build();
};