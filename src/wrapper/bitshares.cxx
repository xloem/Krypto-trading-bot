//#include <graphene/wallet/wallet.hpp>

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
    //graphene::wallet::wallet_data wdata;

    string baseId, quoteId;

    Amount baseScale, quoteScale;

    mutex msgMtx;
    mutex apiMtx;
    uint64_t DATABASE_API_ID;
    map<uint64_t, function<void(uWS::WebSocket<uWS::CLIENT> *, json const &)>> replies;

    static string rpcJson(uint64_t id, int api, string const & method, json::initializer_list_t const & params);
    json rpcCall(int api, string method, json::initializer_list_t const & praams);
    void wsCall(uWS::WebSocket<uWS::CLIENT> *ws, int api, string method, json::initializer_list_t const & params, function<void(uWS::WebSocket<uWS::CLIENT> *,json const &)> handler);

    void error(json const & reply);

    struct mLevelTable : public mLevel {
      vector<mLevel> * table;
    };

    mLevels levels;
    unordered_map<string, mLevelTable> levelsById;

    void handleMarketNotice(json const & objects);
    vector<mLevel>::iterator findLevel(mLevelTable const & level);
    void addLevel(string const & id, Amount amount, Price base, Price quote, Side side);
    void removeLevel(string const & id);
};

}

constexpr int BASE_API_ID = 1;

enum NOTICE_TYPE {
  UNCONFIRMED_TX,
  MARKET,
};

enum OPERATION {
  LIMIT_ORDER_CREATE = 1,
  LIMIT_ORDER_CANCEL = 2,
  CALL_ORDER_UPDATE = 3,
  FILL_ORDER = 4
};

// margin calls are used to maintain the value of a backed currency
// the collateral is being automatically sold for the debt

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

using namespace ₿;
using namespace uWS;
//using namespace graphene::chain;

Gw* new_GwBitshares(string const & exchange)
{
	return new ₿::GwBitshares(exchange);
}

template <typename T>
static T jsonParse(json const & j) {
  try {
    return j.get<T>();
  } catch(json::type_error &) {
    istringstream ss(j.get<string>());
    T ret;
    ss >> ret;
    return ret;
  }
}

₿::GwBitshares::GwBitshares(string const & exch)
{
  http   = "http://127.0.0.1:8090/rpc";
  ws     = "wss://127.0.0.1:8090/ws";
  exchange = exch;
}

const json ₿::GwBitshares::handshake()
{
  //wdata.ws_server = ws;

  try {
    json result;

    result = rpcCall(0, "get_chain_id", {});
    //wdata.chain_id = chain_id_type(result.get<string>());

    result = rpcCall(0, "lookup_asset_symbols", {json::array({base, quote})});
    json const & baseJson = result[0];
    json const & quoteJson = result[1];
    if (baseJson.is_null() || quoteJson.is_null()) {
      std::cerr << "Currency not found." << std::endl;
      throw result;
    }
  
    baseId = baseJson.at("id").get<string>();
    quoteId = quoteJson.at("id").get<string>();
    baseScale = pow(10, jsonParse<Amount>(baseJson.at("precision")));
    quoteScale = pow(10, jsonParse<Amount>(quoteJson.at("precision")));
    makeFee = takeFee = max(jsonParse<Amount>(baseJson.at("options").at("market_fee_percent")),
                            jsonParse<Amount>(quoteJson.at("options").at("market_fee_percent"))) / 100.0;
    minTick = numeric_limits<Price>::epsilon() * 100000;
    minSize = 1.0 / baseScale;

    return {
      {"makeFee", makeFee},
      {"takeFee", takeFee},
      {"minTick", minTick},
      {"minSize", minSize},
      {  "reply", result},
    };
  } catch (json result) {
    return {{  "reply", result}};
  }
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
            wsCall(ws, BASE_API_ID, "login", {apikey, pass}, steps->front());
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());
            if(res != true) throw res;

            wsCall(ws, BASE_API_ID, "database", {}, steps->front());
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());
            DATABASE_API_ID = jsonParse<uint64_t>(res);

            unique_lock<mutex> lock(apiMtx);
            if (baseId.empty()) {
              wsCall(ws, DATABASE_API_ID, "lookup_asset_symbols", {json::array({base, quote})}, steps->front());
            } else {
              steps->splice(steps->end(), *steps, steps->begin()); // skip 1 step
              steps->front()(ws, {});
            }
          },
          [this,steps](WebSocket<CLIENT> *ws, json const & res)
          {
            steps->splice(steps->end(), *steps, steps->begin());

	    cerr << res.dump() << endl;

            unique_lock<mutex> lock(apiMtx);
            json const & baseJson = res[0];
            json const & quoteJson = res[1];
            baseId = baseJson.at("id").get<string>();
            quoteId = quoteJson.at("id").get<string>();
            baseScale = pow(10, jsonParse<Amount>(baseJson.at("precision")));
            quoteScale = pow(10, jsonParse<Amount>(quoteJson.at("precision")));
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
      } catch (std::exception &) {
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
  json const * error = &reply;
  try {
	  error = &error->at("error");
  } catch (json::out_of_range &) { }
#if 0
  cerr << error->dump() << endl;
  /*if (error.is_object()) {
    cerr << error["name"] << ": " << error["message"] << endl;;
    cerr << error["stack"] << endl;
  } else {
    cerr << error << endl;
  }*/
  write_Connectivity(Connectivity::Disconnected);
  // most errors look like this:
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
#endif
}

string ₿::GwBitshares::rpcJson(uint64_t id, int api, string const & method, json::initializer_list_t const & params)
{
  json message = {
    {"id", id},
    {"method", "call"},
    {"params", {
      api,
      method,
      json::array(params)
    }}
  };
  return message.dump();
}

json ₿::GwBitshares::rpcCall(int api, string method, json::initializer_list_t const & params = {})
{
  string msg = rpcJson(0, api, method, params);
  cerr << msg << " -> ";
  json result = Curl::perform(http, [&](CURL *curl) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg.c_str());
  });
  cerr << result.dump() << endl;
  try {
    return result.at("result");
  } catch (json::out_of_range &) {
    error(result);
    throw result["error"];
  }
}

void ₿::GwBitshares::wsCall(WebSocket<CLIENT> *ws, int api, string method, json::initializer_list_t const & params, function<void(uWS::WebSocket<uWS::CLIENT> *, json const &)> handler)
{
  uint64_t id;
  future<json> reply;
  {
    lock_guard<mutex> lock(msgMtx);
    id = replies.empty() ? 0 : 1 + replies.rbegin()->first;
    replies[id] = handler;
  }
  string msg = rpcJson(id, api, method, params);
  ws->send(msg.data(), msg.size(), OpCode::TEXT);
}

vector<mLevel>::iterator ₿::GwBitshares::findLevel(mLevelTable const & level)
{
  return lower_bound(
      level.table->begin(),
      level.table->end(),
      level,
      [](mLevel const & a, mLevel const & b)
      {
        return a.price < b.price;
      });
}

// when base is our base, it's a bid
// when it's an ask, base is our quote
void ₿::GwBitshares::addLevel(string const & id, Amount amount, Price base, Price quote, ₿::Side side)
{
  removeLevel(id); // todo: inline, if it matters, to reduce calls to findLevel(); can level.price change?

  mLevelTable & level = levelsById[id];
  if (side == Side::Bid) {
    level.size = amount / baseScale;
    level.price = quote * baseScale / (quoteScale * base);
    level.table = & levels.bids;
  } else {
    swap(quote,base); // amount is now in rawquote currency
    auto basePerRawQuote = base / (quote * baseScale);
    level.size = amount * basePerRawQuote;
    level.price = 1.0 / (quoteScale * basePerRawQuote);
    level.table = & levels.asks;
  };

  auto levelIt = findLevel(level);
  if (levelIt != level.table->end()) // separation of conditions ensures no segfault due to evaluation order
    if (levelIt->price == level.price) {
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
  auto const bookLevelIt = findLevel(orderLevel);
  if (bookLevelIt != orderLevel.table->end()) {
    bookLevelIt->size -= orderLevel.size;
    if (bookLevelIt->size <= 0)
      orderLevel.table->erase(bookLevelIt);
  }
  levelsById.erase(orderLevelIt);
}
  
// cnvc.org has a database of 600 certified trainers and mediators in the process on the tape
//
//
//        it's very hard to make use of something like that in this situation, all round
//
//  i'm trying to learn it, it's very slow for me
//
//
//        what would you say if somebody were to say:
//              "sorry, we can't help you"
//                    i'd try to infer from the context what feelings or needs might be going on
//                    but short of that, maybe ...
//                are you irritated at all because you need to take care of yourself here?
//
//  so the hope is that by learning this others will find some of it (i think my translators have found some of it)
//  and that i can eventually know it well enough to use in this confusing situations more directly
//
//  what are you working on on the lowre half of the screen?
//      i'm adding bitshares support to a public cryptocurrency trading bot i use
//      bitshares doesn't have trading fees so more money could be made
//          anything wrong with that?
//                not really.
//                it uses cryptocurrency, it builds me money, and it uses my time
//                there's somebody else who took responsibility for doing this, but i don't trust he will do it rapidly
//                  coinbase raised my fees so the faster i do this the more i (and anyobdy else using the bot) profits
//                      i don't know any of the people who use it
//                          don't know many people in general


// two possible plans for bitshares:
//        1. link to wallet lib in overall makefile
//        2. parse wss urls in overall lib to handle a list of websockets

void ₿::GwBitshares::handleMarketNotice(json const & objects)
{
  for (auto const & object : objects) {
    std::cerr << object << std::endl;
    if (object.is_object()) {
      // note that it's quite possible that an order can be _updated_ by being reprovided with the same id
      //   dos this happen?
      //   -> yes, the "for sale" amount changes as the trade is partly filled
      auto id = object.at("id").get<string>();
      if (id[2] != '7') {
	      cerr << "NOT AN ORDER: IGNORING" << endl;
	      continue;
      }
      auto const & price = object.at("sell_price");
      auto const & base = price.at("base");
      auto const & quote = price.at("quote");
      addLevel(
        id,
        jsonParse<Amount>(object.at("for_sale")), // this is the quantity sold at the price
        jsonParse<Price>(base.at("amount")), // these two make the price
        jsonParse<Price>(quote.at("amount")),
        base.at("asset_id").get<string>() == baseId ? Side::Bid : Side::Ask
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
    } else if (object.is_string()) {
      removeLevel(object);
    } else {
      // a fill
      cerr << "MUST BE A FILL: IGNORING" << endl;
        // updates as a result of filling an order from a prior block will be soon posted as normal updates
        // the fill order does not include the total amount of the order, only the price and id
        // and the normal order notification may appear before or after the fill (usually after atm)
        //
        // fill notices refer to previous trades
        // BUT:
        //    trades from the same block will likely appear afterwards
        //    trades that are fully filled from the same block will _not_ appear
        //    trades from previous blocks will likely be removed afterwards
        // NOTE:
        //    in the future taking trades may be broadcast to the client
        // prices represent RATIOS.  actual available base currency is listed in "for_sale" 
        // fill_price: making trade prior to fill: represents exact price
        // receives: quote value given to trader
        // pays: base value taken from trader
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

