/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ihashfunctions.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IHASHFUNCTIONS_H
#define IHASHFUNCTIONS_H

#include <cstddef>
#include <utility>

#include <core/global/iglobal.h>

namespace iShell {

class iChar;
class iByteArray;
class iString;
class iStringRef;
class iStringView;
class iBitArray;
class iLatin1String;

struct IX_CORE_EXPORT iKeyHashFunc
{
    size_t operator()(const iChar& key) const;
    size_t operator()(const iByteArray& key) const;
    size_t operator()(const iString& key) const;
    size_t operator()(const iStringRef& key) const;
    size_t operator()(const iStringView& key) const;
    size_t operator()(const iLatin1String& key) const;
    size_t operator()(const std::pair<int, int>& key) const;
};

struct IX_CORE_EXPORT iKeyEqualFunc
{
    bool operator()(const iChar& key1, const iChar& key2) const;
    bool operator()(const iByteArray& key1, const iByteArray& key2) const;
    bool operator()(const iString& key1, const iString& key2) const;
    bool operator()(const iStringRef& key1, const iStringRef& key2) const;
    bool operator()(const iStringView& key1, const iStringView& key2) const;
    bool operator()(const iLatin1String& key1, const iLatin1String& key2) const;
    bool operator()(const std::pair<int, int>& key1, const std::pair<int, int>& key2) const;
};

} // namespace iShell

#endif // IHASHFUNCTIONS_H
