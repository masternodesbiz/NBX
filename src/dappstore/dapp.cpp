// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dapp.h"

#include <vector>

#include "univalue/lib/univalue_utffilter.h"
#include "utilstrencodings.h"

bool isBase64png(const std::string &data) {
    bool fInvalid = false;
    std::vector<unsigned char> tmpImg = DecodeBase64(data.c_str(), &fInvalid);
    if (fInvalid)
        return false;
    if (tmpImg.size() <= 8 || memcmp(tmpImg.data(), "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a", 8))
        return false;
    return true;
}

bool isValidUtf8(const std::string &data) {
    std::string str;
    JSONUTF8StringFilter filter(str);
    for (const char &c : data)
        filter.push_back(c);
    if (!filter.finalize())
        return false;
    return true;
}

bool DApp::CheckData(std::string *errorStr) const {
    if (!CheckField(name, errorStr)) {
        if (errorStr)
            *errorStr = "dApp name " + *errorStr;
        return false;
    }
    if (!CheckField(url, errorStr)) {
        if (errorStr)
            *errorStr = "dApp url " + *errorStr;
        return false;
    }
    if (!CheckField(blockchain, errorStr)) {
        if (errorStr)
            *errorStr = "dApp blockchain " + *errorStr;
        return false;
    }
    if (!CheckField(description, errorStr)) {
        if (errorStr)
            *errorStr = "dApp description " + *errorStr;
        return false;
    }
    if (!CheckImage(image, errorStr)) {
        if (errorStr)
            *errorStr = "dApp image " + *errorStr;
        return false;
    }
    return true;
}

bool DApp::IsEmpty() const {
    return name.empty() && url.empty() && blockchain.empty() && description.empty() && image.empty();
}

bool DApp::CheckField(const std::string &str, std::string *errorStr) {
    if (str.empty()) {
        if (errorStr)
            *errorStr = "is empty";
        return false;
    }
    if (!isValidUtf8(str)) {
        if (errorStr)
            *errorStr = "is not in UTF-8";
        return false;
    }
    return true;
}

bool DApp::CheckImage(const std::string &str, std::string *errorStr) {
    if (str.empty()) {
        if (errorStr)
            *errorStr = "is empty";
        return false;
    }
    if (!isBase64png(str)) {
        if (errorStr)
            *errorStr = "is not Base64-encoded PNG";
        return false;
    }
    return true;
}
