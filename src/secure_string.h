// Copyright (c) 2018-2019 Netbox.Global

#ifndef BITCOIN_SECURE_STRING_H
#define BITCOIN_SECURE_STRING_H

#include <string>

#include "allocators.h"

// This is exactly like std::string, but with a custom allocator.
typedef std::basic_string<char, std::char_traits<char>, secure_allocator<char>> SecureString;

bool operator==(const std::string &lhs, const SecureString &rhs);

bool operator!=(const std::string &lhs, const SecureString &rhs);

bool operator<(const std::string &lhs, const SecureString &rhs);

SecureString &SecureStringFromString(const std::string str);

#endif //BITCOIN_SECURE_STRING_H
