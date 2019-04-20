#include "common.h"

namespace ₿ {

  class GwExchangeWrapper : public GwExchange {
    public:
/**/  virtual bool ready(uS::Loop *const) = 0;                               // wait for exchange and register data handlers
/**/  virtual void replace(RandId, string) {};                               // call         async orders data from exchange
/**/  virtual void place(RandId, Side, string, string, OrderType, TimeInForce, bool) = 0, // async orders like above / below
/**/               cancel(RandId, RandId) = 0,                               // call         async orders data from exchange
/**/               close() = 0;                                              // disconnect but without waiting for reconnect
/**/protected:
/**/  virtual bool            async_wallet() { return false; };              // call         async wallet data from exchange
/**/  virtual vector<mWallets> sync_wallet() { return {}; };                 // call and read sync wallet data from exchange
/**/  virtual vector<mLevels>  sync_levels() { return {}; };                 // call and read sync levels data from exchange
/**/  virtual vector<mTrade>   sync_trades() { return {}; };                 // call and read sync trades data from exchange
/**/  virtual vector<mOrder>   sync_orders() { return {}; };                 // call and read sync orders data from exchange
/**/  virtual vector<mOrder>   sync_cancelAll() = 0;                         // call and read sync orders data from exchange
  };

}

using namespace ₿;

extern "C" { Gw * new_Gw__wrapped(const std::string & exchange); }

Gw* ₿::Gw::new_Gw(const string& exchange)
{
  if (exchange == "BITSHARES")
    return new_GwBitshares(exchange);
  else
    return new_Gw__wrapped(exchange);
}
