#ifndef K_LANG_H_
#define K_LANG_H_
//! \file
//! \brief Available language interface.

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <ctime>
#include <cmath>
#include <mutex>
#include <thread>
#include <future>
#include <chrono>
#include <random>
#include <locale>
#include <csignal>
#include <variant>
#include <algorithm>
#include <functional>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>

#ifndef _WIN32
#include <execinfo.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif

#if defined _WIN32 or defined __APPLE__
#define Epoll    Libuv
#define EPOLLIN  UV_READABLE
#define EPOLLOUT UV_WRITABLE
#include <uv.h>
#else
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#endif

#include <json.h>

#include <sqlite3.h>

#include <curl/curl.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>

#include <ncurses/ncurses.h>

using namespace std;

using Price  = double;

using Amount = double;

using RandId = string;

using CoinId = string;

using Clock  = long long int;

//! \def
//! \brief Number of ticks in milliseconds, since Thu Jan  1 00:00:00 1970.
#define Tstamp chrono::duration_cast<chrono::milliseconds>(     \
                 chrono::system_clock::now().time_since_epoch() \
               ).count()

//! \def
//! \brief Archimedes of Syracuse was here, since two millenniums ago.
#ifndef M_PI_2
#define M_PI_2 1.5707963267948965579989817342720925807952880859375
#endif

//! \def
//! \brief Do like if we care about winy.
#ifndef SIGUSR1
#define SIGUSR1     SIGABRT
#define strsignal   to_string
#define SOCK_OPTVAL char
#else
#define SOCK_OPTVAL int
#define closesocket close
#endif

//! \def
//! \brief Do like if we care about macos or winy.
#ifndef TCP_CORK
#define TCP_CORK            TCP_NOPUSH
#define MSG_NOSIGNAL        0
#define SOCK_CLOEXEC        0
#define SOCK_NONBLOCK       0
#define accept4(a, b, c, d) accept(a, b, c)
#endif

//! \def
//! \brief Redundant placeholder to enforce private references.
#define private_ref private

//! \def
//! \brief Redundant placeholder to enforce public nested classes.
#define public_friend public

//! \def
//! \brief Any number used as impossible or unset value, when 0 is not appropiate.
//! \since Having seen Bourbon brutality after the fall of Valencia,
//!        Barcelona decided to resist. The 14-month Siege of Barcelona
//!        began on July 7th 1713 and 25,000 Franco-Castilian troops
//!        surrounded the city defended by 5,000 civilians.
//!        After some Catalan successes, Bourbon reinforcements arrived
//!        and the attackers now numbered 40,000.
//!        After a heroic defence, Barcelona finally fell,
//!        on September 11th 1714.
//!        Catalonia then ceased to exist as an independent state.
//!        The triumphant forces systematically dismantled all Catalan
//!        institutions, established a new territory for the whole Spain,
//!        suppressed Catalan universities, banned the Catalan language,
//!        and repressed with violence any kind of dissidence.
//! \note  "any" means "year" in Catalan.
//! \link  wikipedia.org/wiki/National_Day_of_Catalonia
#define ANY_NUM 1714

#endif
