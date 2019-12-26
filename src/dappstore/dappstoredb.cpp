// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dappstoredb.h"

#include "dapp.h"

bool DAppStoreDB::Add(const uint256 &txid, const DAppExt &dApp) {
    return Write(txid, dApp);
}

bool DAppStoreDB::AddHistory(const uint256 &txid, const DApp &dApp) {
    return Write(std::make_pair(std::string("tx"), txid), dApp);
}

bool DAppStoreDB::GetHistory(const uint256 &txid, DApp &dApp) {
    return Read(std::make_pair(std::string("tx"), txid), dApp);
}

bool DAppStoreDB::DeleteHistory(const uint256 &txid) {
    return Erase(std::make_pair(std::string("tx"), txid));
}

bool DAppStoreDB::Delete(const uint256 &txid) {
    return Erase(txid);
}

bool DAppStoreDB::Remove() {
    return bitdb.RemoveDb(strFile);
}

bool DAppStoreDB::Load(std::vector<uint256> &dAppTxs, std::unordered_map <uint256, DAppExt> &dApps, CBlockLocator &bestBlock, CAmount &price) {
    Dbc *pcursor = GetCursor();
    if (!pcursor)
        return false;
    bool fError = false;
    while (true) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue);
        if (ret == DB_NOTFOUND)
            break;
        if (ret) {
            fError = true;
            break;
        }
        if (ssKey.size() == sizeof(uint256)) {
            uint256 txid;
            ssKey >> txid;
            DAppExt dApp;
            ssValue >> dApp;
            dApps[txid] = dApp;
        } else if (ssKey.size() == sizeof(uint256) + 3) { // skip "tx" + uint256
        } else {
            std::string key;
            ssKey >> key;
            if (key == "bestblock") {
                ssValue >> bestBlock;
            } else if (key == "txs") {
                ssValue >> dAppTxs;
            } else if (key == "price") {
                ssValue >> price;
            }
        }
    }
    pcursor->close();
    return !fError;
}

bool DAppStoreDB::WriteBestBlock(const CBlockLocator& bestBlock) {
    return Write(std::string("bestblock"), bestBlock);
}

bool DAppStoreDB::WriteTxs(const std::vector <uint256> &dAppTxs) {
    return Write(std::string("txs"), dAppTxs);
}

bool DAppStoreDB::WritePrice(const CAmount &price) {
    return Write(std::string("price"), price);
}