#include "common.h"

#include "json.h"
#include <list>

namespace ₿ {

class GwBitshares : public ₿::GwApiWS {
  public:
    GwBitshares(string const & exchange);
    virtual bool ready(uS::Loop *const);                               // wait for exchange and register data handlers
    //virtual void replace(RandId, string) {};                           // call         async orders data from exchange
    virtual void place(RandId, Side, string, string, OrderType, TimeInForce, bool) {} // async orders like above / below
    virtual void cancel(RandId, RandId) {}                               // call         async orders data from exchange
    virtual void close() {}                                              // disconnect but without waiting for reconnect
    virtual const json handshake();
  protected:
    //virtual bool            async_wallet();                            // call         async wallet data from exchange
    //virtual vector<mWallets> sync_wallet();                            // call and read sync wallet data from exchange
    //virtual vector<mLevels>  sync_levels();                            // call and read sync levels data from exchange
    //virtual vector<mTrade>   sync_trades();                            // call and read sync trades data from exchange
    //virtual vector<mOrder>   sync_orders();                            // call and read sync orders data from exchange
    virtual vector<mOrder>   sync_cancelAll() { return {}; }                         // call and read sync orders data from exchange

  private:
    uWS::Hub wsHub;

    string baseId, quoteId;

    Amount baseScale, quoteScale;

    mutex msgMtx;
    mutex apiMtx;
    uint64_t DATABASE_API_ID;
    map<uint64_t, function<void(uWS::WebSocket<uWS::CLIENT> *, json const &)>> replies;

    void wsCall(uWS::WebSocket<uWS::CLIENT> *ws, int api, string method, json const & params, function<void(uWS::WebSocket<uWS::CLIENT> *,json const &)> handler);

    void error(json const & reply);

    struct mLevelTable : public mLevel {
      vector<mLevel> * table;
    };

    mLevels levels;
    unordered_map<string, mLevelTable> levelsById;

    void handleMarketNotice(json const & objects);
    void addLevel(string const & id, Amount base, Amount quote, Side side);
    void removeLevel(string const & id);
};

}

constexpr int BASE_API_ID = 1;

enum NOTICE_TYPE {
  MARKET
};

// websocket behavior:
//  askForData -> askForNeverAsyncData
//      async_wallet() or
//      sync_wallet()
//      sync_cancelAll()
//  waitForData -> waitForNeverAsyncData
//      sync_cancelAll -> write_mOrder
//      sync->wallet() ->  write_mWallets
//
// At base, the bot itself assigns to handling functions:
//      write_Connectivity(const Connectivity &)
//      write_mWallets(const mWallets)
//      write_mLevels(const mLevels)
//      write_mOrder(const mOrder)
//      write_mTrade(const mTrade)
// it expects these to be called appropriately.

using namespace uWS;
using namespace ₿;

Gw* new_GwBitshares(string const & exchange)
{
	return new ₿::GwBitshares(exchange);
}

₿::GwBitshares::GwBitshares(string const & exch)
{
  http   = "http://127.0.0.1:8090/rpc";
  ws     = "wss://127.0.0.1:8090/ws";
  exchange = exch;
}

const json ₿::GwBitshares::handshake()
{
  string req = json({
     {"id",0},
     {"method","call"},
     {"params",{
       0,
       "lookup_asset_symbols",
       json::array({json::array({base, quote})})
     }}
    }).dump();
  json result = Curl::perform(http, [&](CURL *curl) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.c_str());
  });
  if (!result["error"].is_null()) {
    error(result);
    return {{  "reply", result}};
  }
  json const & res = result["result"];
  json const & baseJson = res[0];
  json const & quoteJson = res[1];
  if (baseJson.is_null() || quoteJson.is_null()) {
    std::cerr << "Currency not found." << std::endl;
    return {{  "reply", result}};
  }
  {
    lock_guard<mutex> lock(apiMtx);
    
    baseId = baseJson["id"].get<string>();
    quoteId = quoteJson["id"].get<string>();
    minTick = numeric_limits<Price>::epsilon() * 100000;
    baseScale = pow(10, baseJson["precision"].get<Amount>());
    quoteScale = pow(10, quoteJson["precision"].get<Amount>());
    minSize = 1.0 / baseScale;
    makeFee = takeFee = max(baseJson["options"]["market_fee_percent"], quoteJson["options"]["market_fee_percent"]);
  }
  return {
    {"makeFee", makeFee},
    {"takeFee", takeFee},
    {"minTick", minTick},
    {"minSize", minSize},
    {  "reply", result},
  };
}

// wait for exchange and register data handlers
bool ₿::GwBitshares::ready(uS::Loop *const)
{
  api->onConnection([&](WebSocket<CLIENT> *ws, HttpRequest req) {

      shared_ptr<list<function<void(WebSocket<CLIENT> *, json const &)>>> steps = decltype(steps)(new list<function<void(WebSocket<CLIENT> *, json const &)>>());
      steps->assign({
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());
            wsCall(ws, BASE_API_ID, "login", json::array({apikey, pass}), steps->front());
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());
            if(res != true) throw res;

            wsCall(ws, BASE_API_ID, "database", json::array(), steps->front());
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());
            DATABASE_API_ID = res.get<uint64_t>();

            unique_lock<mutex> lock(apiMtx);
            if (baseId.empty()) {
              wsCall(ws, DATABASE_API_ID, "lookup_asset_symbols", json::array({json::array({base, quote})}), steps->front());
            } else {
              steps->splice(steps->end(), *steps, steps->begin()); // skip 1 step
              steps->front()(ws, {});
            }
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());

            unique_lock<mutex> lock(apiMtx);
            json const & baseJson = res[0];
            json const & quoteJson = res[1];
            baseId = baseJson["id"].get<string>();
            quoteId = quoteJson["id"].get<string>();
            steps->front()(ws, {});
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());

            wsCall(ws, DATABASE_API_ID, "subscribe_to_market", { MARKET, base, quote }, steps->front());
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());

            write_Connectivity(Connectivity::Connected);
          }
      });

      steps->front()(ws, {});
  });
  api->onMessage([&](WebSocket<CLIENT> *ws, char * message, size_t size, OpCode opcode) {
    if (opcode != OpCode::TEXT) return;

    json msg = json::parse(message, message + size);
    auto idEntry = msg.find("id");
    if (idEntry != msg.end()) {
      try {
        decltype(replies)::mapped_type handler;
        {
          lock_guard<mutex> lock(msgMtx);
          handler = replies[*idEntry];
        }
        handler(ws, msg.at("result"));
      } catch(...) {
        error(msg);
      }
    } else if (msg["method"] == "notice") {
      lock_guard<mutex> lock(apiMtx);
      auto params = msg["params"];
      NOTICE_TYPE noticeType = params[0];
      auto objects = params[1][0];
      switch(noticeType) {
      case MARKET:
        handleMarketNotice(objects);
        break;
      default:
        cerr << msg.dump() << endl;
      }
    } else {
      cerr << msg.dump() << endl;
    }
  });
  return true;
}

void ₿::GwBitshares::error(json const & reply)
{
  cerr << "BITSHARES ERROR" << endl;
  cerr << reply["error"]["name"] << ": " << reply["error"]["message"] << endl;;
  cerr << reply["error"]["stack"] << endl;
  write_Connectivity(Connectivity::Disconnected);
  // errors look like this:
  // {
  //   "id":0,
  //  "error": {
  //    "data": {
  //      "code": error-code,
  //      "name": " ... exception name "
  //      "message": " ... ",
  //      "stack": [ ... stack trace ... ],
  //    },
  //    "code": 1,
  //  }
  // }
}

void ₿::GwBitshares::wsCall(WebSocket<CLIENT> *ws, int api, string method, json const & params, function<void(uWS::WebSocket<uWS::CLIENT> *, json const &)> handler)
{
  uint64_t id;
  future<json> reply;
  {
    lock_guard<mutex> lock(msgMtx);
    id = replies.empty() ? 0 : 1 + replies.rbegin()->first;
    replies[id] = handler;
  }
  json message = {
    {"id", id},
    {"method", "call"},
    {"params", {
      api,
      method,
      params
    }}
  };
  string msg = message.dump();
  ws->send(msg.data(), msg.size(), OpCode::TEXT);
}

  // note: client code wants to be called whenever levels changes, it
  // seems !
  // so I guess I'll just keep rewriting the vector

void ₿::GwBitshares::addLevel(string const & id, Amount base, Amount quote, ₿::Side side)
{
  mLevelTable level;
  if (side == Side::Bid) {
    level.price = quote * quoteScale / (base * baseScale);
    level.size = quote / baseScale;
    level.table = & levels.bids;
  } else {
    level.price = base * quoteScale / (quote * baseScale);
    level.size = base / baseScale;
    level.table = & levels.asks;
  };
  levelsById[id] = level;

  auto levelIt = lower_bound(level.table->begin(), level.table->end(), level, [](mLevel const & a, mLevel const & b) { return a.price < b.price; });
  if (levelIt != level.table->end())
    if (levelIt->price == level.price) { // separation of conditions ensures no segfault due to evaluation order
      levelIt->size += level.size;
      return;
    }
  // else
  level.table->insert(levelIt, level);
}


void ₿::GwBitshares::removeLevel(string const & id)
{
  auto const orderLevelIt = levelsById.find(id);
  if (orderLevelIt == levelsById.end())
    return;
  auto const & orderLevel = orderLevelIt->second;
  auto const bookLevelIt = lower_bound(orderLevel.table->begin(), orderLevel.table->end(), orderLevel, [](mLevel const & a, mLevel const & b) { return a.price < b.price; });
  if (bookLevelIt != orderLevel.table->end()) {
    bookLevelIt->size -= orderLevel.size;
    if (bookLevelIt->size <= 0)
      orderLevel.table->erase(bookLevelIt);
  }
  levelsById.erase(orderLevelIt);
}
  

void ₿::GwBitshares::handleMarketNotice(json const & objects)
{
  for (auto const & object : objects) {
    std::cerr << object << std::endl;
    if (object.is_object()) {
      auto const & sellPrice = object["sell_price"];
      auto const & base = sellPrice["base"];
      auto const & quote = sellPrice["quote"];
      addLevel(
        object["id"].get<string>(),
        base["amount"].get<Amount>(),
        quote["amount"].get<Amount>(),
        base["asset_id"].get<string>() == baseId ? Side::Ask : Side::Bid
      );
        // orders are placed with object ids
        // when only the object id is received, the order is removed. 
        //
        // {"method":"notice","params":
        // [
        //  3,
        //  [
        //    [
        //      {
        //        "id":"1.7.347570872",
        //        "expiration":"1963-11-29T12:38:45",
        //        "seller":"1.2.33015",
        //        "for_sale":469544265,
        //        "sell_price":
        //        {
        //          "base":
        //          {
        //            "amount":469544265,
        //            "asset_id":"1.3.0"
        //          },
        //          "quote":
        //          {
        //            "amount":1740451,
        //            "asset_id":"1.3.850"
        //          }
        //        },
        //        "deferred_fee":2526,
        //        "deferred_paid_fee":
        //        {
        //          "amount":0,
        //          "asset_id":"1.3.0"
        //        }
        //      }
        //    ]
        //  ]
        //]}
    } else {
      removeLevel(object);
    }
  }
  write_mLevels(levels);
}

//
// wallet-specific commands work only on cli_wallet
// database 
//
// full-node is required for websocket protocol?
// NOTE: bitshares has a "high security setup" that
//   prevents malicious reversing of transactions


// websocket: (can use wscat to test websockets)
// set_subscribe_callback(int identifier, bool clear_filter)
//    set identifier as id for notifications, clear_filter = true
//    will now notify for objects requested
//    {
//      "method": "notice"
//      "params": [
//        identifier,
//        [[
//          {"id": "2.1.0", ... },
//          {"id": ...},
//          ...
//      ]
// subscribe_to_market(int identifier, assed_id a, asset_id b)
// get_full_accounts(array account_ids, bool subscribe)
//    -> also returns full account object for each account

