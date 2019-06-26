#ifndef BITCOIN_MNEMONIC_H
#define BITCOIN_MNEMONIC_H

#include <string>
#include <vector>

#include "secure_string.h"
#include "uint256.h"

extern const std::vector <std::string> mnemonicDictionaryEn;

class CMnemonic {
public:
    CMnemonic(const std::vector <std::string> &dictionary = mnemonicDictionaryEn) : dictionary(dictionary) {}

    bool ParseMnemonic(const std::vector <SecureString> &words, uint256 &seed);

    std::vector <SecureString> GetMnemonic(const uint256 &seed, bool removeZeros = true);

protected:
    const std::vector <std::string> &dictionary;

    int GetWordPos(const SecureString &word);

    static int GetChecksumSize(int bitsCount, bool forDecode = false);

    static uint32_t GetChecksum(const unsigned char* begin, int size, int mask);
};

#endif //BITCOIN_MNEMONIC_H
