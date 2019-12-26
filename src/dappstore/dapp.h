// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAPP_H
#define BITCOIN_DAPP_H

#include <string>
#include <unordered_map>
#include <vector>

#include "script/script.h"
#include "serialize.h"

class DApp {
public:
    DApp() {}

    DApp(const std::string &name,
         const std::string &url,
         const std::string &blockchain,
         const std::string &description,
         const std::string &image) :
            name(name),
            url(url),
            blockchain(blockchain),
            description(description),
            image(image) {}

    bool CheckData(std::string *errorStr = NULL) const;

    bool IsEmpty() const;

    static bool CheckField(const std::string &str, std::string *errorStr = NULL);

    static bool CheckImage(const std::string &str, std::string *errorStr = NULL);

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action, int nType, int nVersion) {
        READWRITE(name);
        READWRITE(url);
        READWRITE(blockchain);
        READWRITE(description);
        READWRITE(image);
        READWRITE(time);
    }

    std::string name;
    std::string url;
    std::string blockchain;
    std::string description;
    std::string image;

    int64_t time = 0;
};

class DAppExt : public DApp {
public:

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action, int nType, int nVersion) {
        DApp::SerializationOp(s, ser_action, nType, nVersion);
        READWRITE(created);
        READWRITE(deleted);
        READWRITE(script);
        READWRITE(updateTxs);
    }

    DAppExt() : DApp() {};
    DAppExt(const DApp &dApp) : DApp(dApp) {};

    int64_t created = 0;
    CScript script;
    bool deleted = false;

    std::vector <uint256> updateTxs;
};

#endif // BITCOIN_DAPP_H
