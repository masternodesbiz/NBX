// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dappstore/dappstore.h"
#include "init.h"
#include "main.h"
#include "masternode-sync.h"
#include "rpc/server.h"
#include "uint256.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <univalue.h>

extern DAppStore* pdAppStore;

extern CAmount GetAccountBalance(const std::string &strAccount, int nMinDepth);

extern void SendMoney(const CTxDestination &address, CAmount nValue, CWalletTx &wtxNew, bool fUseIX = false, bool fAllowZero = false);

uint256 SendMessage(const std::string &type, const std::string &method, const UniValue &data, const CAmount amount, const CTxDestination &address){
    if (nMaxDatacarrierBytes < 83)
        throw JSONRPCError(RPC_MISC_ERROR, "nMaxDatacarrierBytes is less than 83");
    unsigned partSize = nMaxDatacarrierBytes - 4;
    if (partSize > 0xffff - 3)
        partSize -= 4;
    else if (partSize > 0xff - 3)
        partSize -= 2;
    else if (partSize > OP_PUSHDATA1 - 3)
        partSize -= 1;

    if (!IsMine(*pwalletMain, address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "NBX address is not mine");

    EnsureWalletIsUnlocked();

    // prepare data
    UniValue msg(UniValue::VOBJ);
    msg.push_back(Pair("type", type));
    msg.push_back(Pair("method", method));
    msg.pushKVs(data);
    std::string msgStr = msg.write();
    if (msgStr.size() > DATAMSG_MAX_LENGTH)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Data size is greater than 10KB");
    msgStr = deflate(msgStr);
    if (msgStr.size() > DATAMSG_MAX_LENGTH)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Compressed data size is greater than 10KB");
    int partsCount = (msgStr.size() + partSize - 1) / partSize;
    if (amount == 0 && partsCount > 1)
        throw JSONRPCError(RPC_MISC_ERROR, "Free message must fit in one transaction");
    partSize = (msgStr.size() + partsCount - 1) / partsCount;
    if (partSize < DATAMSG_MIN_LENGTH)
        partSize = DATAMSG_MIN_LENGTH;

    // prepare money
    CWalletTx wtx;
    CAmount minFee = pwalletMain->GetMinimumFee(204 + partSize, 25, mempool);
    CAmount nAmount = amount + (partsCount - 1) * minFee;
    CScript addressScript = GetScriptForDestination(address);
    SendMoney(address, nAmount, wtx, false, true);

    // create new transaction
    uint256 txid;
    CMutableTransaction prevTx(wtx);
    for (int i = 0; i < partsCount; i++) {

        CMutableTransaction tx;
        if (i == 0) {
            for (unsigned int j = 0; j < wtx.vout.size(); j++) {
                if (wtx.vout[j].nValue == nAmount && wtx.vout[j].scriptPubKey == addressScript) {
                    CTxIn in(COutPoint(wtx.GetHash(), j), CScript(), std::numeric_limits<uint32_t>::max());
                    tx.vin.push_back(in);
                    break;
                }
            }
            if (tx.vin.empty())
                throw JSONRPCError(RPC_WALLET_ERROR, "Error creating new transaction");
        } else {
            CTxIn in(COutPoint(txid, 1), CScript(), std::numeric_limits<uint32_t>::max());
            tx.vin.push_back(in);
        }

        std::vector<unsigned char> tmpData{DATAMSG_PREFIX, DATAMSG_SUBPREFIX_ADDITIONAL};
        tmpData.insert(tmpData.end(), msgStr.begin() + (partsCount - i - 1) * partSize, ((i == 0) ? msgStr.end() : msgStr.begin() + (partsCount - i) * partSize));

        nAmount -= minFee;
        if (i < partsCount - 1) {
            tx.vout.resize(2);
            tx.vout[1].nValue = nAmount;
            tx.vout[1].scriptPubKey = addressScript;
        } else {
            tmpData[1] = DATAMSG_SUBPREFIX_MAIN;
            tx.vout.resize(1);
        }
        tx.vout[0].nValue = 0;
        tx.vout[0].scriptPubKey = CScript() << OP_RETURN << tmpData;

        // sign transaction
        if (!SignSignature(*pwalletMain, prevTx, tx, 0))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error signing new transaction");
        txid = tx.GetHash();

        // check transaction size
        {
            CTransaction tmpTx(tx);
            unsigned int nBytes = GetSerializeSize(*(CTransaction*)&tmpTx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBytes >= MAX_STANDARD_TX_SIZE)
                throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction is too large");
        }

        // send transaction
        CCoinsViewCache &view = *pcoinsTip;
        const CCoins *existingCoins = view.AccessCoins(txid);
        bool fHaveMempool = mempool.exists(txid);
        bool fHaveChain = existingCoins && existingCoins->nHeight < 1000000000;
        if (!fHaveMempool && !fHaveChain) {
            // push to local node and sync with wallets
            CValidationState state;
            bool fMissingInputs;
            if (!AcceptToMemoryPool(mempool, state, tx, false, &fMissingInputs, false)) {
                if (state.IsInvalid()) {
                    throw JSONRPCError(RPC_TRANSACTION_REJECTED, strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
                } else {
                    if (fMissingInputs) {
                        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
                    }
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, state.GetRejectReason());
                }
            }
        } else if (fHaveChain) {
            throw JSONRPCError(RPC_TRANSACTION_ALREADY_IN_CHAIN, "Transaction already in block chain");
        }
        RelayTransaction(tx);
        prevTx = tx;
    }

    return txid;
}

UniValue addnewdapp(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() < 5 || params.size() > 6)
        throw std::runtime_error(
                "addnewdapp \"name\" \"url\" \"blockchain\" \"description\" \"image\" ( \"fromaccount\" )\n"

                "\nNOTE: By default this function only works sometimes. This is when the tx is in the mempool\n"
                "or there is an unspent output in the utxo for this transaction. To make it always work,\n"
                "you need to maintain a transaction index, using the -txindex command line option.\n"

                "\nAdds new dApp to dApp Store.\n"

                "\nArguments:\n"
                "1. \"name\"        (string, required) DApp name\n"
                "2. \"url\"         (string, required) URL to dApp\n"
                "3. \"blockchain\"  (string, required) Blockchain on which dApp is based\n"
                "4. \"description\" (string, required) DApp description\n"
                "5. \"image\"       (string, required) DApp image (Base64-encoded)\n"
                "6. \"address\"     (string, optional) NBX address to create dApp\n"

                "\nResult:\n"
                "\"hex\"            (string) The transaction hash in hex\n"

                "\nExamples:\n"
                + HelpExampleCli("addnewdapp", "\"My dApp\" \"https://example.com/\" \"Ethereum\" \"DApp Description\" \"Base-64 encoded image data\"")
                + HelpExampleCli("addnewdapp", "\"My dApp\" \"https://example.com/\" \"Ethereum\" \"DApp Description\" \"Base-64 encoded image data\" \"NYkDC3hHouRaCSAMNsSxFj5Xv1h5etPzGT\"")
                + HelpExampleRpc("addnewdapp", "\"My dApp\", \"https://example.com/\", \"Ethereum\", \"DApp Description\", \"Base-64 encoded image data\"")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    if (!masternodeSync.IsBlockchainSynced())
        throw std::runtime_error("Blockchain is not synced yet");

    // check data
    DApp newDApp(params[0].get_str(), params[1].get_str(), params[2].get_str(), params[3].get_str(), params[4].get_str());
    {
        std::string errorStr;
        if (!newDApp.CheckData(&errorStr))
            throw JSONRPCError(RPC_INVALID_PARAMETER, errorStr);
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // prepare data
    UniValue dAppData(UniValue::VOBJ);
    dAppData.push_back(Pair("name", newDApp.name));
    dAppData.push_back(Pair("url", newDApp.url));
    dAppData.push_back(Pair("bc", newDApp.blockchain));
    dAppData.push_back(Pair("descr", newDApp.description));
    dAppData.push_back(Pair("img", newDApp.image));

    // get address
    CTxDestination address;
    if (params.size() > 5) {
        CBitcoinAddress tmpAddress(params[5].get_str());
        if (!tmpAddress.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid NBX address");
        address = tmpAddress.Get();
        if (!IsMine(*pwalletMain, address))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "NBX address is not mine");
    } else {
        if (!pwalletMain->IsLocked())
            pwalletMain->TopUpKeyPool();

        CReserveKey reservekey(pwalletMain);
        CPubKey vchPubKey;
        if (!reservekey.GetReservedKey(vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Keypool ran out, please call keypoolrefill first");

        reservekey.KeepKey();
        CKeyID keyID = vchPubKey.GetID();

        CBitcoinAddress tmpAddress;
        tmpAddress.Set(keyID);
        address = tmpAddress.Get();
    }

    uint256 txid = SendMessage("dapp", "new", dAppData, DAPPSTORE_COMISSION_ADD * pdAppStore->GetPrice(), address);

    return txid.GetHex();
}

UniValue dAppToJson(const uint256 &txid, const DAppExt &dApp, bool hide = false) {
    UniValue entry(UniValue::VOBJ);
    entry.push_back(Pair("txid", txid.GetHex()));
    entry.push_back(Pair("name", dApp.name));
    entry.push_back(Pair("url", dApp.url));
    entry.push_back(Pair("blockchain", dApp.blockchain));
    if (!hide) {
        entry.push_back(Pair("description", dApp.description));
        entry.push_back(Pair("image", dApp.image));
    } else {
        UniValue updates(UniValue::VARR);
        for (auto tx : dApp.updateTxs) {
            if (tx != txid)
                updates.push_back(tx.GetHex());
        }
        if (!updates.empty())
            entry.push_back(Pair("updates", updates));
    }
    entry.push_back(Pair("created", dApp.created));
    entry.push_back(Pair("time", dApp.time));
    if (dApp.deleted)
        entry.push_back(Pair("deleted", true));
    return entry;
}

UniValue dappdelete(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "dappdelete \"txid\"\n"

                "\nNOTE: By default this function only works sometimes. This is when the tx is in the mempool\n"
                "or there is an unspent output in the utxo for this transaction. To make it always work,\n"
                "you need to maintain a transaction index, using the -txindex command line option.\n"

                "\nDelete dApp from dApp Store.\n"

                "\nArguments:\n"
                "1. \"txid\"      (string, required) The transaction id\n"

                "\nResult:\n"
                "\"hex\"          (string) The transaction hash in hex\n"

                "\nExamples:\n"
                + HelpExampleCli("dappdelete", "\"mytxid\"")
                + HelpExampleRpc("dappdelete", "\"mytxid\"")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    if (!masternodeSync.IsBlockchainSynced())
        throw std::runtime_error("Blockchain is not synced yet");

    uint256 txid = ParseHashV(params[0], "txid");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // get input script
    DAppExt *dApp = NULL;
    for (auto dAppTx : pdAppStore->dAppMyTxs)
        if (dAppTx == txid) {
            dApp = &pdAppStore->dApps[txid];
            break;
        }
    if (!dApp)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "DApp not found or it is not mine");

    if (dApp->deleted)
        throw std::runtime_error("DApp is already deleted");

    // prepare data
    CTxDestination address;
    if (!ExtractDestination(dApp->script, address))
        throw std::runtime_error("Can't extract destination from previous script");
    UniValue dAppData(UniValue::VOBJ);
    dAppData.push_back(Pair("txid", txid.GetHex()));

    txid = SendMessage("dapp", "del", dAppData, 0.0001 * COIN, address);

    return txid.GetHex();
}

UniValue dappupdate(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() != 6)
        throw std::runtime_error(
                "dappupdate \"txid\" \"name\" \"url\" \"blockchain\" \"description\" \"image\"\n"

                "\nNOTE: By default this function only works sometimes. This is when the tx is in the mempool\n"
                "or there is an unspent output in the utxo for this transaction. To make it always work,\n"
                "you need to maintain a transaction index, using the -txindex command line option.\n"

                "\nUpdate dApp in dApp Store.\n"

                "\nArguments:\n"
                "1. \"txid\"        (string, required) The transaction id\n"
                "2. \"name\"        (string, required) DApp name\n"
                "3. \"url\"         (string, required) URL to dApp\n"
                "4. \"blockchain\"  (string, required) Blockchain on which dApp is based\n"
                "5. \"description\" (string, required) DApp description\n"
                "6. \"image\"       (string, required) DApp image (Base64-encoded)\n"

                "\nResult:\n"
                "\"hex\"          (string) The transaction hash in hex\n"

                "\nExamples:\n"
                + HelpExampleCli("dappupdate", "\"mytxid\" \"https://example.com/\" \"Ethereum\" \"DApp Description\" \"Base-64 encoded image data\"")
                + HelpExampleRpc("dappupdate", "\"mytxid\", \"https://example.com/\", \"Ethereum\", \"DApp Description\", \"Base-64 encoded image data\"")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    if (!masternodeSync.IsBlockchainSynced())
        throw std::runtime_error("Blockchain is not synced yet");

    uint256 txid = ParseHashV(params[0], "txid");

    // check data
    DApp newDApp(params[1].get_str(), params[2].get_str(), params[3].get_str(), params[4].get_str(), params[5].get_str());
    {
        std::string errorStr;
        if (!newDApp.CheckData(&errorStr))
            throw JSONRPCError(RPC_INVALID_PARAMETER, errorStr);
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // get input script
    DAppExt *dApp = NULL;
    for (auto dAppTx : pdAppStore->dAppMyTxs)
        if (dAppTx == txid) {
            dApp = &pdAppStore->dApps[txid];
            break;
        }
    if (!dApp)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "DApp not found or it is not mine");

    if (dApp->deleted)
        throw std::runtime_error("DApp is deleted");

    // prepare data
    UniValue dAppData(UniValue::VOBJ);
    CTxDestination address;
    if (!ExtractDestination(dApp->script, address))
        throw std::runtime_error("Can't extract destination from previous script");
    dAppData.push_back(Pair("txid", txid.GetHex()));
    bool changed = false;
    if (newDApp.name != dApp->name) {
        dAppData.push_back(Pair("name", newDApp.name));
        changed = true;
    }
    if (newDApp.url != dApp->url) {
        dAppData.push_back(Pair("url", newDApp.url));
        changed = true;
    }
    if (newDApp.blockchain != dApp->blockchain) {
        dAppData.push_back(Pair("bc", newDApp.blockchain));
        changed = true;
    }
    if (newDApp.description != dApp->description) {
        dAppData.push_back(Pair("descr", newDApp.description));
        changed = true;
    }
    if (newDApp.image != dApp->image) {
        dAppData.push_back(Pair("img", newDApp.image));
        changed = true;
    }

    if(!changed)
        throw std::runtime_error("DApp is not changed");

    txid = SendMessage("dapp", "upd", dAppData, DAPPSTORE_COMISSION_UPDATE * pdAppStore->GetPrice(), address);

    return txid.GetHex();
}

UniValue getdapp(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "getdapp \"txid\"\n"

                "\nGet dApp info.\n"

                "\nArguments:\n"
                "1. \"txid\"      (string, required) The transaction id\n"

                "\nResult:\n"
                "{\n"
                "  \"txid\": \"xxxx\",           (string) Txid with dApp info\n"
                "  \"name\": \"xxxx\",           (string) DApp name\n"
                "  \"url\": \"xxxx\",            (string) URL to dApp\n"
                "  \"blockchain\": \"xxxx\",     (string) Blockchain on which dApp is based\n"
                "  \"description\": \"xxxx\",    (string) DApp description\n"
                "  \"image\": \"xxxx\"           (string) DApp image (Base64-encoded)\n"
                "  \"deleted\": true             (bool, optional) True if application was deleted\n"
                "}\n"

                "\nExamples:\n"
                + HelpExampleCli("getdapp", "\"mytxid\"")
                + HelpExampleRpc("getdapp", "\"mytxid\"")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    uint256 dAppTx = ParseHashV(params[0], "txid");

    LOCK(cs_main);

    if (!pdAppStore->dApps.count(dAppTx))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "DApp transaction not found");

    return dAppToJson(dAppTx, pdAppStore->dApps[dAppTx]);
}

UniValue getdappprice(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "getdappprice\n"

                "\nGet dApp creating/modifying prices.\n"

                "\nResult:\n"
                "{\n"
                "  \"add\": 1000,   (numeric) DApp creation price in NBX\n"
                "  \"update\": 100  (numeric) DApp modification price in NBX\n"
                "}\n"

                "\nExamples:\n"
                + HelpExampleCli("getdappprice", "")
                + HelpExampleRpc("getdappprice", "")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    LOCK(cs_main);

    UniValue res(UniValue::VOBJ);
    res.push_back(Pair("add", ValueFromAmount(pdAppStore->GetPrice() * DAPPSTORE_COMISSION_ADD)));
    res.push_back(Pair("update", ValueFromAmount(pdAppStore->GetPrice() * DAPPSTORE_COMISSION_UPDATE)));

    return res;
}

UniValue listdapps(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
                "listdapps ( verbose )\n"

                "\nLists all dApp's in dApp Store.\n"

                "\nArguments:\n"
                "1. verbose     (bool, optional, default=false) If false, return txid's list, otherwise return list with description\n"

                "\nResult (if verbose is not set or set to false):\n"
                "[\n"
                "  \"txid\"      (string) DApp txid\n"
                "]\n"

                "\nResult (if verbose is set to true):\n"
                "[\n"
                "  {\n"
                "    \"txid\": \"xxxx\",           (string) Txid with dApp info\n"
                "    \"name\": \"xxxx\",           (string) DApp name\n"
                "    \"url\": \"xxxx\",            (string) URL to dApp\n"
                "    \"blockchain\": \"xxxx\",     (string) Blockchain on which dApp is based\n"
                "    \"description\": \"xxxx\",    (string) DApp description\n"
                "    \"image\": \"xxxx\"           (string) DApp image (Base64-encoded)\n"
                "  }\n"
                "]\n"

                "\nExamples:\n"
                + HelpExampleCli("listdapps", "")
                + HelpExampleCli("listdapps", "true")
                + HelpExampleRpc("listdapps", "")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    int fVerbose = 0;
    if (!params[0].isNull())
        fVerbose = params[0].isNum() ? params[0].get_int() : (params[0].get_bool() ? 1 : 0);

    LOCK(cs_main);

    UniValue ret(UniValue::VARR);
    for (auto dAppTx : pdAppStore->dAppTxs)
        if (!pdAppStore->dApps[dAppTx].deleted || fVerbose == 2) {
            if (fVerbose)
                ret.push_back(dAppToJson(dAppTx, pdAppStore->dApps[dAppTx], fVerbose == 2));
            else
                ret.push_back(dAppTx.GetHex());
        }

    return ret;
}

UniValue listmydapps(const UniValue &params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
                "listmydapps ( verbose )\n"

                "\nLists my dApp's.\n"

                "\nArguments:\n"
                "1. verbose     (bool, optional, default=false) If false, return txid's list, otherwise return list with description\n"

                "\nResult (if verbose is not set or set to false):\n"
                "[\n"
                "  \"txid\"      (string) DApp txid\n"
                "]\n"

                "\nResult (if verbose is set to true):\n"
                "[\n"
                "  {\n"
                "    \"txid\": \"xxxx\",           (string) Txid with dApp info\n"
                "    \"name\": \"xxxx\",           (string) DApp name\n"
                "    \"url\": \"xxxx\",            (string) URL to dApp\n"
                "    \"blockchain\": \"xxxx\",     (string) Blockchain on which dApp is based\n"
                "    \"description\": \"xxxx\",    (string) DApp description\n"
                "    \"image\": \"xxxx\"           (string) DApp image (Base64-encoded)\n"
                "  }\n"
                "]\n"

                "\nExamples:\n"
                + HelpExampleCli("listmydapps", "")
                + HelpExampleCli("listmydapps", "true")
                + HelpExampleRpc("listmydapps", "")
        );

    if (!pdAppStore)
        throw std::runtime_error("DApp Store is disabled. Start with -dappstore to enable it");

    int fVerbose = 0;
    if (!params[0].isNull())
        fVerbose = params[0].isNum() ? params[0].get_int() : (params[0].get_bool() ? 1 : 0);

    LOCK(cs_main);

    UniValue ret(UniValue::VARR);
    for (auto dAppTx : pdAppStore->dAppMyTxs)
        if (!pdAppStore->dApps[dAppTx].deleted || fVerbose == 2) {
            if (fVerbose)
                ret.push_back(dAppToJson(dAppTx, pdAppStore->dApps[dAppTx], fVerbose == 2));
            else
                ret.push_back(dAppTx.GetHex());
        }

    return ret;
}
