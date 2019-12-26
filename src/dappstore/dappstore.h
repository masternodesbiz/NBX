// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAPPSTORE_H
#define BITCOIN_DAPPSTORE_H

#define DAPPSTORE_COMISSION_ADD 10
#define DAPPSTORE_COMISSION_UPDATE 1
#define DATAMSG_MIN_LENGTH 78
#define DATAMSG_MAX_LENGTH 10240
#define DATAMSG_PREFIX 27
#define DATAMSG_SUBPREFIX_MAIN 1
#define DATAMSG_SUBPREFIX_ADDITIONAL 2

#include <unordered_map>
#include <string>
#include <vector>

#include "chain.h"
#include "dapp.h"
#include "dappstoredb.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "uint256.h"
#include "zlib.h"

class DAppStore {
public:
    DAppStore(const std::string &file);

    CBlockLocator GetBestBlock();

    bool SetBestBlock(const CBlockLocator &bestBlock);

    CAmount GetPrice();

    int ScanForTransactions(CBlockIndex *pindexStart);

    int ParseVtx(std::vector <CTransaction> &vtx, int64_t blockTime);

    int CancelVtx(std::vector <CTransaction> &vtx);

    std::unordered_map <uint256, DAppExt> dApps;
    std::vector <uint256> dAppTxs;
    std::vector <uint256> dAppMyTxs;

protected:
    bool Add(const uint256 &txid, const DApp &dApp, const CScript &script);

    bool Remove(const uint256 &dAppId, const uint256 &txid, const CScript &script, int64_t time);

    bool Update(const uint256 &dAppId, const uint256 &txid, const CScript &script, const DApp &dApp);

    bool ApplyUpdate(DAppExt &dApp, const DApp &dAppNew);

    bool IsUpdatable(const uint256 &dAppId, const uint256 &txid, const CScript &script, const DApp &dApp);

    bool AddTransaction(const uint256 &dAppId, const uint256 &txid, const DApp &dApp, DAppExt *dAppExt = NULL);

    bool AddIsMine(const uint256 &txid, const CScript &script);

    void RecalculateDApp(const uint256 &txid);

    bool SaveTxs();

    bool SetPrice(const CAmount &price);

    std::string dbFile;
    CBlockLocator bestBlock;
    CAmount price = 10;

    std::unordered_map <uint256, uint256> dAppHistoryTxs;
};

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

#endif // BITCOIN_DAPPSTORE_H
