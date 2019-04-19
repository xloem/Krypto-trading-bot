#include "common.h"

#include "json.h"

namespace ₿ {

class GwBitshares : public ₿::GwExchange {
  public:
    GwBitshares();
    virtual bool ready(uS::Loop *const);                               // wait for exchange and register data handlers
    virtual void replace(RandId, string) {};                           // call         async orders data from exchange
    virtual void place(RandId, Side, string, string, OrderType, TimeInForce, bool), // async orders like above / below
                 cancel(RandId, RandId),                               // call         async orders data from exchange
                 close();                                              // disconnect but without waiting for reconnect
  protected:
    virtual bool            async_wallet();                            // call         async wallet data from exchange
    virtual vector<mWallets> sync_wallet();                            // call and read sync wallet data from exchange
    virtual vector<mLevels>  sync_levels();                            // call and read sync levels data from exchange
    virtual vector<mTrade>   sync_trades();                            // call and read sync trades data from exchange
    virtual vector<mOrder>   sync_orders();                            // call and read sync orders data from exchange
    virtual vector<mOrder>   sync_cancelAll();                         // call and read sync orders data from exchange

  private:
    uWS::Hub wsHub;

    std::mutex msgMtx;
    uint64_t dbId;
    std::map<uint64_t, std::promise<json>> replies;

    json wsCall(uWS::WebSocket<uWS::CLIENT> *ws, std::string method, json params);

    void error(json msg);
};

}

using namespace uWS;

₿::GwBitshares::GwBitshares()
{
  http   = "http://127.0.0.1:8090/rpc";
  ws     = "wss://127.0.0.1:8090/ws";
}

// wait for exchange and register data handlers
bool ₿::GwBitshares::ready(uS::Loop *const)
{
  api->onConnection([&](WebSocket<CLIENT> *ws, HttpRequest req) {
    json res = wsCall(ws, "login", json::array({apikey, pass}));
    if (res["result"] != true) error(res);
    res = wsCall(ws, "database", json::array());
    dbId = res["result"];
  });
  api->onMessage([&](WebSocket<CLIENT> *ws, char * message, size_t size, OpCode opcode) {
    if (opcode != OpCode::TEXT) return;

    json msg = json::parse(message, message + size);
    auto idEntry = msg.find("id");
    if (idEntry != msg.end()) {
      std::lock_guard<std::mutex> lock(msgMtx);
      replies[*idEntry].set_value(msg);
    } else {
      std::cerr << msg.dump() << std::endl;
    }
  });
}

json ₿::GwBitshares::wsCall(WebSocket<CLIENT> *ws, std::string method, json params)
{
  uint64_t id;
  std::future<json> reply;
  {
    std::lock_guard<std::mutex> lock(msgMtx);
    id = replies.empty() ? 0 : 1 + replies.rbegin()->first;
    reply = replies[id].get_future();
  }
  json message = {
    {"id", id},
    {"method", "call"},
    {"params", {
      1,
      method,
      params
    }}
  };
  std::string msg = message.dump();
  ws->send(msg.data(), msg.size(), OpCode::TEXT);
  json ret = reply.get();
  {
    std::lock_guard<std::mutex> lock(msgMtx);
    replies.erase(id);
  }
  return id;
}

// erorrs look like this:
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
//}
//
// wallet-specific commands owrk only on cli_wallet
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

