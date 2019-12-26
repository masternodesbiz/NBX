// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dappstore.h"

#include <map>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <string.h>
#include <univalue.h>

#include "checkpoints.h"
#include "guiinterface.h"
#include "init.h"
#include "main.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/wallet_ismine.h"
#endif

const CScript &GetDAppPubKey() {
    static CScript *dAppPubKey = NULL;
    if (!dAppPubKey) {
        CBitcoinAddress dAppAddress;
        assert(dAppAddress.SetString(Params().DAppAddress()));
        dAppPubKey = new CScript(GetScriptForDestination(dAppAddress.Get()));
    }
    return *dAppPubKey;
}

DAppStore::DAppStore(const std::string &file) {
    dbFile = file;
    DAppStoreDB *db = NULL;
    try {
        db = new DAppStoreDB(dbFile, "cr+");
    } catch (...) {
        boost::filesystem::remove(GetDataDirForDb() + dbFile);
        db = new DAppStoreDB(dbFile, "cr+");
    }
    if (!db)
        throw std::runtime_error("Error opening dApp Store file \"" + dbFile + "\"");
    try {
        if (!db->Load(dAppTxs, dApps, bestBlock, price))
            throw std::runtime_error("Error loading dApps list from file \"" + dbFile + "\"");
    } catch (...) {
        db->Remove();
        delete db;
        bestBlock.SetNull();
        dApps.clear();
        dAppTxs.clear();
        price = 10;
        db = new DAppStoreDB(dbFile, "cr+");
        if (!db->Load(dAppTxs, dApps, bestBlock, price)) {
            delete db;
            throw std::runtime_error("Error loading dApps list from file \"" + dbFile + "\"");
        }
    }
    delete db;
    for (auto dAppTx : dAppTxs) {
        if (!dApps.count(dAppTx))
            throw std::runtime_error("Error checking dApps list from file \"" + dbFile + "\"");
        AddIsMine(dAppTx, dApps[dAppTx].script);
        for (auto tx : dApps[dAppTx].updateTxs)
            dAppHistoryTxs[tx] = dAppTx;
    }
}

bool DAppStore::Add(const uint256 &txid, const DApp &dApp, const CScript &script) {
    if (!dApp.CheckData())
        return false;
    DAppExt tmpDApp = dApp;
    tmpDApp.script = script;
    tmpDApp.created = tmpDApp.time;
    if (AddTransaction(txid, txid, dApp, &tmpDApp) && DAppStoreDB(dbFile).Add(txid, tmpDApp)) {
        bool existing = dApps.count(txid);
        dApps[txid] = tmpDApp;
        if (!existing) {
            dAppTxs.push_back(txid);
            AddIsMine(txid, script);
        }
        return true;
    }
    return false;
}

bool DAppStore::Remove(const uint256 &dAppId, const uint256 &txid, const CScript &script, int64_t time) {
    DApp delDApp;
    delDApp.time = time;
    if (!IsUpdatable(dAppId, txid, script, delDApp))
        return false;
    DAppExt tmpDApp = dApps[dAppId];
    tmpDApp.deleted = true;
    tmpDApp.time = time;
    if (DAppStoreDB(dbFile).Add(dAppId, tmpDApp)) {
        dApps[dAppId] = tmpDApp;
        return true;
    }
    return false;
}

bool DAppStore::Update(const uint256 &dAppId, const uint256 &txid, const CScript &script, const DApp &dApp) {
    if (dApp.IsEmpty())
        return false;
    if (!IsUpdatable(dAppId, txid, script, dApp))
        return false;
    DAppExt tmpDApp = dApps[dAppId];
    ApplyUpdate(tmpDApp, dApp);
    if (DAppStoreDB(dbFile).Add(dAppId, tmpDApp)) {
        dApps[dAppId] = tmpDApp;
        return true;
    }
    return false;
}

bool DAppStore::ApplyUpdate(DAppExt &dApp, const DApp &dAppNew) {
    if (dApp.deleted)
        return false;
    dApp.time = dAppNew.time;
    if (dAppNew.IsEmpty()) {
        dApp.deleted = true;
        return true;
    }
    if (!dAppNew.name.empty())
        dApp.name = dAppNew.name;
    if (!dAppNew.url.empty())
        dApp.url = dAppNew.url;
    if (!dAppNew.blockchain.empty())
        dApp.blockchain = dAppNew.blockchain;
    if (!dAppNew.description.empty())
        dApp.description = dAppNew.description;
    if (!dAppNew.image.empty())
        dApp.image = dAppNew.image;
    return true;
}

bool DAppStore::IsUpdatable(const uint256 &dAppId, const uint256 &txid, const CScript &script, const DApp &dApp) {
    if (!dApps.count(dAppId))
        return false;
    if (dApps[dAppId].script != script)
        return false;
    if (!AddTransaction(dAppId, txid, dApp))
        return false;
    if (dApps[dAppId].deleted)
        return false;
    return true;
}

bool DAppStore::AddTransaction(const uint256 &dAppId, const uint256 &txid, const DApp &dApp, DAppExt *dAppExt) {
    if (dAppHistoryTxs.count(txid))
        return false;
    dAppHistoryTxs[txid] = dAppId;
    bool ret = true;
    if (dAppExt)
        dAppExt->updateTxs.push_back(txid);
    else {
        dApps[dAppId].updateTxs.push_back(txid);
        ret = DAppStoreDB(dbFile).Add(dAppId, dApps[dAppId]);
    }
    return DAppStoreDB(dbFile).AddHistory(txid, dApp) && ret;
}

CBlockLocator DAppStore::GetBestBlock() {
    return bestBlock;
}

bool DAppStore::SetBestBlock(const CBlockLocator &bestBlock) {
    if (DAppStoreDB(dbFile).WriteBestBlock(bestBlock)) {
        this->bestBlock = bestBlock;
        return true;
    }
    return false;
}

CAmount DAppStore::GetPrice() {
    return price;
}

int DAppStore::ScanForTransactions(CBlockIndex *pindexStart) {
    int ret = 0;
    int64_t nNow = GetTime();

    CBlockIndex *pindex = pindexStart;
    LOCK(cs_main);

    uiInterface.ShowProgress("Rescanning dApp Store...", 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
    double dProgressStart = Checkpoints::GuessVerificationProgress(pindex, false);
    double dProgressTip = Checkpoints::GuessVerificationProgress(chainActive.Tip(), false);
    while (pindex) {
        if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0)
            uiInterface.ShowProgress("Rescanning dApp Store... ", std::max(1, std::min(99, (int) ((Checkpoints::GuessVerificationProgress(pindex, false) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));

        CBlock block;
        ReadBlockFromDisk(block, pindex);
        ret += ParseVtx(block.vtx, block.nTime);

        pindex = chainActive.Next(pindex);
        if (GetTime() >= nNow + 60) {
            nNow = GetTime();
            LogPrintf("Still rescanning dApp Store. At block %d. Progress=%f\n", pindex->nHeight, Checkpoints::GuessVerificationProgress(pindex));
        }
    }
    uiInterface.ShowProgress("Rescanning dApp Store...", 100); // hide progress dialog in GUI
    return ret;
}

int DAppStore::ParseVtx(std::vector<CTransaction> &vtx, int64_t blockTime) {
    int ret = 0;
    for (const CTransaction &tx : vtx) {
        if (tx.vout.size() == 1 && tx.vin.size() == 1 && tx.vout[0].nValue == 0 && tx.vout[0].scriptPubKey.size() > 3 && tx.vout[0].scriptPubKey[0] == OP_RETURN) {
            CTransaction tmpTx = tx;
            GzipInflate gzInflate;
            bool decoded = false;
            bool firstTx = true;
            bool msgSigned = false;
            CScript prevScript;
            CAmount prevAmount = 0;
            size_t dataSize = 0;
            do {
                // check additional data
                CScript::const_iterator pc = tmpTx.vout[0].scriptPubKey.begin();
                opcodetype opcode;
                std::vector<unsigned char> vch;
                if (!tmpTx.vout[0].scriptPubKey.GetOp(pc, opcode, vch) || opcode != OP_RETURN)
                    break;
                if (!tmpTx.vout[0].scriptPubKey.GetOp(pc, opcode, vch) || opcode <= 0 || opcode > OP_PUSHDATA4)
                    break;
                if (vch.size() < 3 || vch[0] != DATAMSG_PREFIX || vch[1] != (firstTx ? DATAMSG_SUBPREFIX_MAIN : DATAMSG_SUBPREFIX_ADDITIONAL))
                    break;
                // check message size
                dataSize += vch.size() - 2;
                if (dataSize > DATAMSG_MAX_LENGTH)
                    break;
                // check previous transaction
                uint256 hashBlock;
                uint256 prevHash = tmpTx.vin[0].prevout.hash;
                size_t prevOut = tmpTx.vin[0].prevout.n;
                if (!GetTransaction(prevHash, tmpTx, hashBlock, true))
                    break;
                if (tmpTx.vout.size() <= prevOut)
                    break;
                if (firstTx) {
                    prevScript = tmpTx.vout[prevOut].scriptPubKey;
                    prevAmount = tmpTx.vout[prevOut].nValue;
                    msgSigned = prevScript == GetDAppPubKey();
                }
                if (gzInflate.Append(std::string(vch.begin() + 2, vch.end()))) {
                    decoded = true;
                    break;
                }
                if (gzInflate.IsError() || gzInflate.Size() > DATAMSG_MAX_LENGTH) {
                    decoded = false;
                    break;
                }
                if (vch.size() - 2 < DATAMSG_MIN_LENGTH)
                    break;
                if (!msgSigned && prevAmount < DAPPSTORE_COMISSION_UPDATE * price)
                    break;
                if (tmpTx.vout.size() != 2 || prevOut != 1 || tmpTx.vin.size() != 1 || tmpTx.vout[0].scriptPubKey.size() <= 3 || tmpTx.vout[0].scriptPubKey[0] != OP_RETURN)
                    break;
                firstTx = false;
            } while (true);
            if (decoded) {
                std::string msgStr = gzInflate.Get();
                UniValue msg;
                if (msg.read(msgStr) && msg.isObject()) {
                    std::map <std::string, UniValue::VType> msgKeys = {
                            {"type",   UniValue::VSTR},
                            {"method", UniValue::VSTR},
                    };
                    if (msg.checkObject(msgKeys)) {
                        std::string msgType = msg["type"].get_str();
                        std::string msgMethod = msg["method"].get_str();
                        if (msgType == "dapp") {
                            if (msgMethod == "new" && (prevAmount >= DAPPSTORE_COMISSION_ADD * price || msgSigned)) { // add new
                                std::map <std::string, UniValue::VType> dAppKeys = {
                                        {"name",  UniValue::VSTR},
                                        {"url",   UniValue::VSTR},
                                        {"bc",    UniValue::VSTR},
                                        {"descr", UniValue::VSTR},
                                        {"img",   UniValue::VSTR},
                                };
                                if (msg.checkObject(dAppKeys)) {
                                    DApp newDApp(msg["name"].get_str(),
                                                 msg["url"].get_str(),
                                                 msg["bc"].get_str(),
                                                 msg["descr"].get_str(),
                                                 msg["img"].get_str());
                                    newDApp.time = blockTime;
                                    if (Add(tx.GetHash(), newDApp, prevScript)) {
                                        LogPrintf("Found new application %s\n", tx.GetHash().GetHex().c_str());
                                        ret++;
                                    }
                                }
                            } else if (msgMethod == "upd" && (prevAmount >= DAPPSTORE_COMISSION_UPDATE * price || msgSigned)) { // update
                                std::map <std::string, UniValue::VType> dAppKeys = {
                                        {"txid", UniValue::VSTR},
                                };
                                if (msg.checkObject(dAppKeys)) {
                                    uint256 txid;
                                    txid.SetHex(msg["txid"].get_str());
                                    DApp updatedDApp;
                                    size_t index;
                                    if (msg.findKey("name", index) && msg[index].isStr() && DApp::CheckField(msg[index].get_str()))
                                        updatedDApp.name = msg[index].get_str();
                                    if (msg.findKey("url", index) && msg[index].isStr() && DApp::CheckField(msg[index].get_str()))
                                        updatedDApp.url = msg[index].get_str();
                                    if (msg.findKey("bc", index) && msg[index].isStr() && DApp::CheckField(msg[index].get_str()))
                                        updatedDApp.blockchain = msg[index].get_str();
                                    if (msg.findKey("descr", index) && msg[index].isStr() && DApp::CheckField(msg[index].get_str()))
                                        updatedDApp.description = msg[index].get_str();
                                    if (msg.findKey("img", index) && msg[index].isStr() && DApp::CheckImage(msg[index].get_str()))
                                        updatedDApp.image = msg[index].get_str();
                                    updatedDApp.time = blockTime;
                                    Update(txid, tx.GetHash(), prevScript, updatedDApp);
                                }
                            } else if (msgMethod == "del") { // delete
                                std::map <std::string, UniValue::VType> dAppKeys = {
                                        {"txid", UniValue::VSTR},
                                };
                                if (msg.checkObject(dAppKeys)) {
                                    uint256 txid;
                                    txid.SetHex(msg["txid"].get_str());
                                    Remove(txid, tx.GetHash(), prevScript, blockTime);
                                }
                            } else if (msgMethod == "prc" && msgSigned) { // price
                                int64_t val = -1;
                                try {
                                    val = msg["val"].get_int64();
                                } catch (...) {}
                                if (val >= 0)
                                    SetPrice(val);
                            }
                        }
                    }
                }
            }
        }
    }
    if (ret)
        SaveTxs();
    return ret;
}

int DAppStore::CancelVtx(std::vector <CTransaction> &vtx) {
    int ret = 0;
    for (const CTransaction &tx : vtx) {
        uint256 txid = tx.GetHash();
        if (dApps.count(txid)) {
            if (DAppStoreDB(dbFile).Delete(txid)) {
                for (auto updateTx : dApps[txid].updateTxs) {
                    if (dAppHistoryTxs.count(updateTx))
                        dAppHistoryTxs.erase(updateTx);
                    DAppStoreDB(dbFile).DeleteHistory(updateTx);
                }
                dApps.erase(txid);
                auto indexTx = std::find(dAppTxs.begin(), dAppTxs.end(), txid);
                assert(indexTx != dAppTxs.end());
                dAppTxs.erase(indexTx);
                auto indexMyTx = std::find(dAppMyTxs.begin(), dAppMyTxs.end(), txid);
                if (indexMyTx != dAppMyTxs.end())
                    dAppMyTxs.erase(indexMyTx);
                ret++;
            }
        } else if (dAppHistoryTxs.count(txid)) {
            uint256 dAppId = dAppHistoryTxs[txid];
            if (dApps.count(dAppId)){
                auto indexTx = std::find(dApps[dAppId].updateTxs.begin(), dApps[dAppId].updateTxs.end(), txid);
                assert(indexTx != dApps[dAppId].updateTxs.end());
                dApps[dAppId].updateTxs.erase(indexTx);
                RecalculateDApp(dAppId);
                DAppStoreDB(dbFile).Add(dAppId, dApps[dAppId]);
            }
            dAppHistoryTxs.erase(txid);
            DAppStoreDB(dbFile).DeleteHistory(txid);
        }
    }
    if (ret)
        SaveTxs();
    return ret;
}

bool DAppStore::AddIsMine(const uint256 &txid, const CScript &script) {
    if (!pwalletMain)
        return false;
    if (IsMine(*pwalletMain, script)) {
        dAppMyTxs.push_back(txid);
        return true;
    }
    return false;
}

void DAppStore::RecalculateDApp(const uint256 &txid) {
    DAppExt tmpDApp = dApps[txid];
    tmpDApp.deleted = false;
    for (auto tx : tmpDApp.updateTxs) {
        DApp txDApp;
        DAppStoreDB(dbFile).GetHistory(tx, txDApp);
        ApplyUpdate(tmpDApp, txDApp);
    }
    dApps[txid] = tmpDApp;
}

bool DAppStore::SaveTxs() {
    return DAppStoreDB(dbFile).WriteTxs(dAppTxs);
}

bool DAppStore::SetPrice(const CAmount &price) {
    this->price = price;
    return DAppStoreDB(dbFile).WritePrice(price);
}

GzipInflate::~GzipInflate() {
    if (initialized) {
        delete buf;
        inflateEnd(&stream);
    }
}

void GzipInflate::Initialize() {
    if (Z_OK != inflateInit2(&stream, -15))
        throw std::runtime_error("Error initializing gzip inflate");
    buf = new char[10240];
    initialized = true;
}

bool GzipInflate::Append(const std::string &data) {
    if (!initialized)
        Initialize();
    stream.next_in = (Bytef *) data.data();
    stream.avail_in = data.length();
    int ret;
    do {
        stream.next_out = (Bytef *) buf;
        stream.avail_out = 10240;
        ret = inflate(&stream, Z_NO_FLUSH);
        if (Z_STREAM_ERROR == ret || Z_NEED_DICT == ret || Z_DATA_ERROR == ret || Z_MEM_ERROR == ret) {
            isError = true;
            return true;
        }
        res += std::string(buf, 10240 - stream.avail_out);
    } while (stream.avail_out == 0);
    return ret == Z_STREAM_END;
}

std::string GzipInflate::Get() {
    return res;
}

size_t GzipInflate::Size() {
    return res.size();
}

bool GzipInflate::IsError() {
    return isError;
}

std::string deflate(const std::string &data) {
    if (data.empty())
        return "";
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    if (Z_OK != deflateInit2(&stream, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY))
        return "";
    char *buf = new char[10240];
    stream.next_in = (Bytef *) data.data();
    stream.avail_in = data.length();
    std::string res;
    do {
        stream.next_out = (Bytef *) buf;
        stream.avail_out = 10240;
        if (Z_STREAM_ERROR == deflate(&stream, Z_FINISH)) {
            res = "";
            break;
        }
        res.append(buf, 10240 - stream.avail_out);
    } while (stream.avail_out == 0);
    delete buf;
    deflateEnd(&stream);
    return res;
}
