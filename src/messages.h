// Copyright (c) 2019-2020 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MESSAGES_H
#define BITCOIN_MESSAGES_H

#define DATAMSG_MAX_LENGTH 10240
#define DATAMSG_PREFIX 27
#define DATAMSG_SUBPREFIX_SYSTEM 0xff
#define DATAMSG_SUBPREFIX_MAIN 1
#define DATAMSG_SUBPREFIX_ADDITIONAL 2

#include "chain.h"
#include "primitives/block.h"
#include "zlib.h"

void ParseBlockMessages(const CBlock& block, CBlockIndex* pindex);

class GzipInflate {
public:
    ~GzipInflate();

    bool Append(const std::string &data);

    std::string Get();

    size_t Size();

    bool IsError();

protected:
    void Initialize();

    bool initialized = false;
    z_stream stream = {};
    char *buf = NULL;
    std::string res;
    bool isError = false;
};

std::string deflate(const std::string &data);

#endif // BITCOIN_MESSAGES_H
