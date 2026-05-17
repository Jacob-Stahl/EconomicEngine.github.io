#pragma once

#include <string>

// TODO order builder
// TODO use string view for order builder somehow

const int MAX_ASSET_LENGTH = 12;

struct Spread{
    bool bidsMissing = true;
    bool asksMissing = true;

    unsigned short highestBid = 0;
    unsigned short lowestAsk = 0;
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

struct Order{

    /// @brief Id of the trader that placed this order
    long traderId;
    /// @brief Unique id of this order
    long ordId;
    /// @brief Buy or Sell
    Side side;
    /// @brief Quantity of the order.
    unsigned int qty;
    /// @brief Price of the order in cents.
    unsigned short price;
    /// @brief Stop price of the order.
    unsigned short stopPrice;
    /// @brief Asset
    std::string asset;
    /// @brief Order type (Market, Limit, Stop).
    OrdType type;

    // TODO add group number for bulk matching

    /// @brief Time the order was recieved by the service
    unsigned long ordNum;
    /// @brief Number of items filled. 
    unsigned int fill = 0;

    /// @brief Determine if the order should be treated as a market order based on the current market price.
    /// @param marketPrice 
    /// @return 
    bool treatAsMarket(const Spread& spread) const;

    /// @brief Determine if the order should be treated as a limit order based on the current market price
    /// @param spread 
    /// @return 
    bool treatAsLimit(const Spread& spread) const;

    bool fillComplete() const {
        return qty == fill;
    }

    unsigned int unfilled() const {
        // A bit dangerous. unfilled should NEVER be negative
        return qty - fill;
    }

    Order() = default;

    Order(
        std::string asset_,
        Side side_,
        OrdType type_,
        unsigned short price_ = 0,
        unsigned int qty_ = 0,
        unsigned short stopPrice_ = 0
    ){

        // Limit the size of asset name lengths to take advantage of small string optimization (SSO)
        if(asset_.length() > MAX_ASSET_LENGTH){
            std::__throw_length_error("asset exceeds maximum length");
        }

        asset = asset_;
        side = side_;
        type = type_;
        price = price_;
        qty = qty_;
        stopPrice = stopPrice_;
    }
};


enum TimeInForce{
    /// @brief Good Til Cancelled
    GTC,

    /// @brief Immediate or Cancelled
    IOC,

    /// @brief Fill or Kill
    FOC
};

struct Order2{
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
    Order2 order{};
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
        
        Order2 build();
};