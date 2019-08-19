// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BACKTRACE_H
#define BITCOIN_BACKTRACE_H

#include <string>

#ifdef WIN32
#include <windows.h>
#include <vector>
#endif

class CBacktrace {
public:
#ifdef WIN32
    static std::string GetWinBacktrace(LPCONTEXT context = NULL);
#else
    static std::string GetLinBacktrace();
#endif
};

#endif // BITCOIN_BACKTRACE_H
