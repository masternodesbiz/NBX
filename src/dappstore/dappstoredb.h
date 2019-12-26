// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAPPSTOREDB_H
#define BITCOIN_DAPPSTOREDB_H

#include <unordered_map>

#include "dapp.h"
#include "uint256.h"
#include "wallet/db.h"
#include "primitives/block.h"

class DAppStoreDB : public CDB {
public:
    DAppStoreDB(const std::string &strFilename, const char *pszMode = "r+") : CDB(strFilename, pszMode) {}

    bool Add(const uint256 &txid, const DAppExt &dApp);

    bool AddHistory(const uint256 &txid, const DApp &dApp);

    bool GetHistory(const uint256 &txid, DApp &dApp);

    bool DeleteHistory(const uint256 &txid);

    bool Delete(const uint256 &txid);

    bool Remove();

    bool Load(std::vector <uint256> &dAppTxs, std::unordered_map <uint256, DAppExt> &dApps, CBlockLocator &bestBlock, CAmount &price);

    bool WriteBestBlock(const CBlockLocator &bestBlock);

    bool WriteTxs(const std::vector <uint256> &dAppTxs);

    bool WritePrice(const CAmount &price);
};

#endif // BITCOIN_DAPPSTOREDB_H
