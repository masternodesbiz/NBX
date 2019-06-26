// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "secure_string.h"

bool operator==(const std::string &lhs, const SecureString &rhs) {
    std::string::size_type __size = lhs.size();
    if (__size != rhs.size())
        return false;
    return SecureString::traits_type::compare(lhs.data(), rhs.data(), __size) == 0;
}

bool operator!=(const std::string &lhs, const SecureString &rhs) {
    return !(lhs == rhs);
}

bool operator<(const std::string &lhs, const SecureString &rhs) {
    const std::string::size_type __size = lhs.size();
    const SecureString::size_type __osize = rhs.size();
    const SecureString::size_type __len = std::min(__size, __osize);
    const int __r = SecureString::traits_type::compare(lhs.data(), rhs.data(), __len);
    return __r < 0 || (__r == 0 && __size < __osize);
}

SecureString &SecureStringFromString(const std::string str) {
    return (new SecureString)->assign(str.data(), str.size());
}
