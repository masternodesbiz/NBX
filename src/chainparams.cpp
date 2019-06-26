// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//    timestamp before)
// + Contains no strange transactions
static Checkpoints::MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of(400, uint256("0x4aa3e45c4f66dbc6d81c6c6fda4d5699438fd9d81e41c38743392d71cbb29103"))
;
static const Checkpoints::CCheckpointData data = {
        &mapCheckpoints,
        1561556148, // * UNIX timestamp of last checkpoint block
        802,          // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
        2000        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of(21600, uint256("0x9045dbda66b3fac9fe82b372ecfafdd9e5af9f2f0ad9da03ea244f557d782d49"));
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1561544438,
    44000,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of(0, uint256("0xdfbb61c239ee0a99bb47a8c253d98553d70d63e4cad30c584a7b810d388f8cfe"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1560246000,
    0,
    100};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0xa5;
        pchMessageStart[1] = 0x49;
        pchMessageStart[2] = 0x54;
        pchMessageStart[3] = 0x9e;
        vAlertPubKey = ParseHex("04a67a8e05479682559b3ad170cee586d3180008f682078fdf64bed483f1ee78c13637dc403b69af04c73d1fa71c40dbd5ad5d841dee4dcb61cf2b9f294b49bc72");
        nDefaultPort = 28734;
        bnProofOfWorkLimit = ~uint256(0) >> 20;
        nMaxReorganizationDepth = 100;
        nMinerThreads = 0;
        nTargetTimespan = 1 * 60; // 1 day
        nTargetSpacing = 1 * 60;  // 1 minute
        nMaturity = 100;
        nMasternodeCountDrift = 20;
        nMaxMoneyOut = 500000 * COIN;

        /** Height or Time Based Activations **/
        nLastPOWBlock = 200;

        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         */
        const char* pszTimestamp = "BBC 11/Jun/2019 Human staff will always be needed, Amazon insists";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0x1e0ffff0 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 5;
        genesis.nTime = 1561534000;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 0x12413c;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000000077c8f4eabb532fdacc3eb56825d3bcf1401e4958b9a2317cc7d8b0496"));
        assert(genesis.hashMerkleRoot == uint256("0xc8146c6cbb18785c3f29f717a0ef7ff072fdaac51fbe179f062ef63466993e6e"));

        vSeeds.push_back(CDNSSeedData("seed1", "seed1.netbox.global"));
        vSeeds.push_back(CDNSSeedData("seed2", "seed2.netbox.global"));
        vSeeds.push_back(CDNSSeedData("seed1r", "seed1.netboxglobal.com"));
        vSeeds.push_back(CDNSSeedData("seed2r", "seed2.netboxglobal.com"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 53); // 'N'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 38); // 'G'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 0x5c);   // '4' or 'E'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x66)(0x1e)(0xe6)(0xa1).convert_to_container<std::vector<unsigned char> >(); // "NgPub"
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x66)(0x1e)(0xe2)(0x65).convert_to_container<std::vector<unsigned char> >(); // "NgPrv"

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = false;

        nPoolMaxTransactions = 3;
        vSporkKey = ParseHex("0496753303ca6fc00fc57ce3d10fb3e3d9438b3cc15dd2f46c7ccde7073ee97dbb2e268d6bfddad0c2cbe2f0200fa77dc816a12c59aaea4854e8d46f65c8dbfacf");
        strObfuscationPoolDummyAddress = "NLqkWWh4qXkgqquqTPsjYYMGkZhAiGtXNT";

        activityAddress = "NV8C9AYfmoiU8Sd2dBPCsqWchmnh8F6yry";
        teamAddress = "NYkDC3hHouRaCSAMNsSxFj5Xv1h5etPzGT";
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0xa5;
        pchMessageStart[1] = 0x48;
        pchMessageStart[2] = 0x54;
        pchMessageStart[3] = 0x9e;
        vAlertPubKey = ParseHex("0491bc8bf683ecfd94ccd5c97ef0b7ecb5c35d2e84347eb7bd44594d7cb6c78ad4fde7da6af4897d08fb9a7c4b4a2fc2028b44c2df9cda60dad5168768526ed491");
        nDefaultPort = 28754;
        nMinerThreads = 0;
        nTargetTimespan = 1 * 60; // 1 day
        nTargetSpacing = 1 * 60;  // 1 minute
        nLastPOWBlock = 200;
        nMaturity = 15;
        nMasternodeCountDrift = 4;
        nMaxMoneyOut = 500000 * COIN;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1560246000;
        genesis.nNonce = 0xef9336;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000000279920da0376356ee345b9319d1b20df30506f427416fc18831a539d0d"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("seed1Testnet", "seed1.testnet.netbox.global"));
        vSeeds.push_back(CDNSSeedData("seed2Testnet", "seed2.testnet.netbox.global"));
        vSeeds.push_back(CDNSSeedData("seed1TestnetReserve", "seed2.testnet.netboxglobal.com"));
        vSeeds.push_back(CDNSSeedData("seed2TestnetReserve", "seed2.testnet.netboxglobal.com"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 112); // 'n'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 97);  // 'g'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 0xe7);    // '8' or 'b'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x66)(0x20)(0x4d)(0xcb).convert_to_container<std::vector<unsigned char> >(); // "NgTpb"
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x66)(0x20)(0x4e)(0x32).convert_to_container<std::vector<unsigned char> >(); // "NgTpr"

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        vSporkKey = ParseHex("04381890d4abcd72d93429dfb397c9f1803584e680bb13eb27db0603cbbaa474da963a53cdd36e2df77819cce8874c3ada7e4d717ac961738dcb8051224b97e72f");
        strObfuscationPoolDummyAddress = "nU1o33JGU72Mi8gubxz3JYKHbQkXfozsYc";

        activityAddress = "n5q6183ZL3FJsYQfR4NLwRQqP1oas2SdZw";
        teamAddress = "nEnRsJtxh9k4fTtiQrt7TKpphT42asgLhZ";
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xa5;
        pchMessageStart[1] = 0x47;
        pchMessageStart[2] = 0x54;
        pchMessageStart[3] = 0x9e;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60; // 1 day
        nTargetSpacing = 1 * 60;        // 1 minutes
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        nLastPOWBlock = 250;
        nMaturity = 100;
        nMasternodeCountDrift = 4;
        nMaxMoneyOut = 43199500 * COIN;

        //! Modify the regtest genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1560246000;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 0x11062019;

        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 28774;
        assert(hashGenesisBlock == uint256("0xdfbb61c239ee0a99bb47a8c253d98553d70d63e4cad30c584a7b810d388f8cfe"));

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Testnet mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fSkipProofOfWorkCheck = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 51478;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks) { fDefaultConsistencyChecks = afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) { fAllowMinDifficultyBlocks = afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
