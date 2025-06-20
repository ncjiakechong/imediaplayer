/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ihashfunctions.h
/// @brief   defines hash functions for various classes, 
///          enabling their use as keys in hash tables
/// @version 1.0
/// @author  ncjiakechong@gmail.com
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
class iStringView;
class iBitArray;
class iLatin1String;

struct IX_CORE_EXPORT iKeyHashFunc
{
    size_t operator()(const iChar& key) const;
    size_t operator()(const iByteArray& key) const;
    size_t operator()(const iString& key) const;
    size_t operator()(const iStringView& key) const;
    size_t operator()(const iLatin1String& key) const;
    size_t operator()(const std::pair<int, int>& key) const;
};

} // namespace iShell

#endif // IHASHFUNCTIONS_H
