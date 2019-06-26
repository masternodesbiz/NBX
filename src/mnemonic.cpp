// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mnemonic.h"

#include <assert.h>
#include <tgmath.h>
#include <algorithm>

#include "hash.h"

const int dictionaryWordsCount = 2048;
const int bitsPerWord = log2(dictionaryWordsCount);
const int dictionaryBitMask = (1 << bitsPerWord) - 1;
const int mnemonicWordsDiv = 3;
const int chekcsumBitsDiv = 32;

bool CMnemonic::ParseMnemonic(const std::vector <SecureString> &words, uint256 &seed) {
    seed.SetNull();
    if ((words.size() % mnemonicWordsDiv) != 0 || words.empty() || words.size() > 24)
        return false;

    uint512 tmp;

    for (const SecureString &word: words) {
        int wordPos = GetWordPos(word);
        if (wordPos == -1)
            return false;

        tmp <<= bitsPerWord;
        tmp |= wordPos;
    }

    int checksumSize = GetChecksumSize(words.size() * bitsPerWord, true);
    int checksumMask = (1 << checksumSize) - 1;
    uint32_t checksum = tmp.Get32() & checksumMask;
    tmp >>= checksumSize;

    if (checksum != GetChecksum(tmp.begin(), 32, checksumMask))
        return false;

    seed = tmp.trim256();
    tmp.SetNull();

    return true;
}

std::vector <SecureString> CMnemonic::GetMnemonic(const uint256 &seed, bool removeZeros) {
    int checksumSize = GetChecksumSize(seed.size() * 8, false);
    int checksumMask = (1 << checksumSize) - 1;

    uint512 tmpSeed(seed);
    tmpSeed <<= checksumSize;
    tmpSeed |= GetChecksum(seed.begin(), seed.size(), checksumMask);

    std::vector <SecureString> res;
    int wordsCount = (seed.size() * 8 + checksumSize) / bitsPerWord;
    for (int i = 0; i < wordsCount; i++) {
        uint32_t wordIndex = tmpSeed.Get32() & dictionaryBitMask;
        res.insert(res.begin(), SecureStringFromString(this->dictionary[wordIndex]));
        tmpSeed >>= bitsPerWord;
    }

    tmpSeed.SetNull();

    return res;
}

int CMnemonic::GetWordPos(const SecureString &word) {
    std::vector<std::string>::const_iterator pos = std::lower_bound(this->dictionary.begin(), this->dictionary.end(), word);
    if (pos == this->dictionary.end() || *pos != word)
        return -1;
    size_t res = pos - this->dictionary.begin();
    pos = this->dictionary.end();
    return res;
}

int CMnemonic::GetChecksumSize(int bitsCount, bool forDecode) {
    return bitsCount / (chekcsumBitsDiv + (forDecode ? 1 : 0));
}

uint32_t CMnemonic::GetChecksum(const unsigned char *begin, int size, int mask) {
    SecureString reversedSeed(begin, begin + size);
    std::reverse(reversedSeed.begin(), reversedSeed.end());
    uint256 sha256;
    Hash((void *) reversedSeed.data(), reversedSeed.size(), (unsigned char *) &sha256);
    return sha256.Get32() & mask;
}
