// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"
#include "rpc/server.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "util/bip32.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "wallet.h"

#include <fstream>
#include <secp256k1.h>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <openssl/aes.h>
#include <openssl/sha.h>

#include <univalue.h>

std::string static EncodeDumpTime(int64_t nTime)
{
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

std::string static EncodeDumpString(const std::string& str)
{
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string& str)
{
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos + 2 < str.length()) {
            c = (((str[pos + 1] >> 6) * 9 + ((str[pos + 1] - '0') & 15)) << 4) |
                ((str[pos + 2] >> 6) * 9 + ((str[pos + 2] - '0') & 15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

UniValue dumpprivkey(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "dumpprivkey \"nbxaddress\"\n"
            "\nReveals the private key corresponding to 'nbxaddress'.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"nbxaddress\"      (string, required) NBX address for the private key\n"

            "\nResult:\n"
            "\"key\"                (string) The private key\n"

            "\nExamples:\n" +
            HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            HelpExampleRpc("dumpprivkey", "\"myaddress\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    std::string strAddress = params[0].get_str();
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid NBX address");
    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CKey vchSecret;
    if (!pwalletMain->GetKey(keyID, vchSecret))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    return CBitcoinSecret(vchSecret).ToString();
}


UniValue dumpwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "dumpwallet \"filename\"\n"
            "\nDumps all wallet keys in a human-readable format.\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"filename\"    (string, required) The filename\n"

            "\nExamples:\n" +
            HelpExampleCli("dumpwallet", "\"test\"") +
            HelpExampleRpc("dumpwallet", "\"test\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    boost::filesystem::path filepath = params[0].get_str().c_str();
    filepath = boost::filesystem::absolute(filepath);

    std::ofstream file;
    file.open(params[0].get_str().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CKeyID, int64_t> mapKeyBirth;
    const std::map<CKeyID, int64_t>& mapKeyPool = pwalletMain->GetAllReserveKeys();
    pwalletMain->GetKeyBirthTimes(mapKeyBirth);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end(); it++) {
        vKeyBirth.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by Netbox.Wallet %s (%s)\n", CLIENT_BUILD, CLIENT_DATE);
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(chainActive.Tip()->GetBlockTime()));
    file << "\n";

    // add the base58check encoded extended master if the wallet uses HD
    CKeyID seed_id = pwalletMain->GetHDChain().seed_id;
    if (!seed_id.IsNull())
    {
        CKey seed;
        if (pwalletMain->GetKey(seed_id, seed)) {
            CExtKey masterKey;
            masterKey.SetMaster(seed.begin(), seed.size());

            file << "# extended private masterkey: " << CBitcoinExtKey(masterKey).ToString() << "\n\n";
        }
    }

    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID& keyid = it->second;
        std::string strTime = EncodeDumpTime(it->first);
        std::string strAddr = CBitcoinAddress(keyid).ToString();
        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            file << strprintf("%s %s ", CBitcoinSecret(key).ToString(), strTime);
            if (pwalletMain->mapAddressBook.count(keyid)) {
                file << strprintf("label=%s", EncodeDumpString(pwalletMain->mapAddressBook[keyid].name));
            } else if (keyid == seed_id) {
                file << "hdseed=1";
            } else if (mapKeyPool.count(keyid)) {
                file << "reserve=1";
            } else if (pwalletMain->mapKeyMetadata[keyid].hdKeypath == "s") {
                file << "inactivehdseed=1";
            } else {
                file << "change=1";
            }
            file << strprintf(" # addr=%s%s\n", strAddr, (pwalletMain->mapKeyMetadata[keyid].has_key_origin ? " hdkeypath=" + WriteHDKeypath(pwalletMain->mapKeyMetadata[keyid].key_origin.path) : ""));
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();

    UniValue reply(UniValue::VOBJ);
    reply.push_back(Pair("filename", filepath.string()));

    return reply;
}
