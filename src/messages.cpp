// Copyright (c) 2019-2020 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.h"

#define MIN_DYNAMIC_MULTIPLIER 1 // 0.01
#define MAX_DYNAMIC_MULTIPLIER 1000000 // 10k

#include <univalue.h>
#include "base58.h"
#include "main.h"
#include "zlib.h"

#include <unordered_map>

const CScript &GetDynamicRewardPubKey() {
    static CScript *dynamicRewardPubKey = NULL;
    if (!dynamicRewardPubKey) {
        CBitcoinAddress dynamicRewardAddress;
        assert(dynamicRewardAddress.SetString(Params().DynamicRewardAddress()));
        dynamicRewardPubKey = new CScript(GetScriptForDestination(dynamicRewardAddress.Get()));
    }
    return *dynamicRewardPubKey;
}

void ParseBlockMessages(const CBlock &block, CBlockIndex *pindex) {
    CCoinsViewCache view(pcoinsTip);
    std::unordered_map<uint256, const CTransaction *> txsMap;
    for (unsigned int i = 0; i < block.vtx.size(); i++)
        txsMap[block.vtx[i].GetHash()] = &block.vtx[i];
    for (const CTransaction &tx : block.vtx) {
        if (tx.vout.size() == 1 && tx.vin.size() == 1 && tx.vout[0].nValue == 0 && tx.vout[0].scriptPubKey.size() > 3 && tx.vout[0].scriptPubKey[0] == OP_RETURN) {
            CScript::const_iterator pc = tx.vout[0].scriptPubKey.begin();
            opcodetype opcode;
            std::vector<unsigned char> vch;
            if (!tx.vout[0].scriptPubKey.GetOp(pc, opcode, vch) || opcode != OP_RETURN)
                continue;
            if (!tx.vout[0].scriptPubKey.GetOp(pc, opcode, vch) || opcode <= 0 || opcode > OP_PUSHDATA4)
                continue;
            if (vch.size() < 3 || vch[0] != DATAMSG_PREFIX || vch[1] != DATAMSG_SUBPREFIX_SYSTEM)
                continue;
            if (vch.size() - 2 > DATAMSG_MAX_LENGTH)
                continue;
            // check previous transaction
            uint256 prevHash = tx.vin[0].prevout.hash;
            size_t prevN = tx.vin[0].prevout.n;
            CTxOut prevOut;
            auto prevTxIt = txsMap.find(prevHash);
            if (prevTxIt != txsMap.end()) {
                assert(prevN <= (*prevTxIt).second->vout.size());
                prevOut = (*prevTxIt).second->vout[prevN];
            } else {
                const CCoins *coins = view.AccessCoins(prevHash);
                if (coins && coins->IsAvailable(prevN))
                    prevOut = view.GetOutputFor(tx.vin[0]);
                else
                    GetOutput(prevHash, prevN, prevOut);
            }
            if (prevOut.scriptPubKey != GetDynamicRewardPubKey())
                continue;
            // decode message
            GzipInflate gzInflate;
            if (!gzInflate.Append(std::string(vch.begin() + 2, vch.end())))
                continue;
            if (gzInflate.IsError() || gzInflate.Size() > DATAMSG_MAX_LENGTH)
                continue;
            std::string msgStr = gzInflate.Get();
            // read message
            UniValue msg;
            if (msg.read(msgStr) && msg.isObject()) {
                std::map<std::string, UniValue::VType> msgKeys = {
                        {"type",  UniValue::VSTR},
                        {"value", UniValue::VNUM},
                };
                if (msg.checkObject(msgKeys)) {
                    std::string msgType = msg["type"].get_str();
                    if (msgType == "sdm") {
                        int msgValue = msg["value"].get_int();
                        if (msgValue >= MIN_DYNAMIC_MULTIPLIER && msgValue <= MAX_DYNAMIC_MULTIPLIER)
                            pindex->nDynamicMultiplier = msgValue;
                    }
                }
            }
        }
    }
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
