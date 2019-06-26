// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2018 The PIVX developers
// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"

#include "tinyformat.h"

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    if (nSize > 0)
        nSatoshisPerK = nFeePaid * 1000 / nSize;
    else
        nSatoshisPerK = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
    CAmount nFee = nSatoshisPerK * nSize / 1000;

    if (nFee == 0 && nSatoshisPerK > 0)
        nFee = nSatoshisPerK;

    return nFee;
}

CAmount CFeeRate::GetNonZeroFee(size_t nSize) const
{
    CAmount nSatoshisPerK_ = nSatoshisPerK ? : 10000;
    CAmount nFee = nSatoshisPerK_ * nSize / 1000;

    if (nFee == 0 && nSatoshisPerK_ > 0)
        nFee = nSatoshisPerK_;

    return nFee;
}

std::string CFeeRate::ToString() const
{
    return strprintf("%d.%08d NBX/kB", nSatoshisPerK / COIN, nSatoshisPerK % COIN);
}
