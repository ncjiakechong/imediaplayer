/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ihashfunctions.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <hash_set>

#include <core/utils/ichar.h>
#include <core/utils/istring.h>
#include <core/utils/istringview.h>
#include <core/utils/ibytearray.h>
#include <core/utils/ibitarray.h>
#include <core/utils/ihashfunctions.h>

namespace iShell {

static inline size_t hash_internal(const char *p, size_t len, uint seed)
{
    size_t h = seed;

    for (size_t i = 0; i < len; ++i)
        h = 31 * h + p[i];

    return h;
}

static inline size_t hash_internal(const iChar *p, size_t len, uint seed)
{
    size_t h = seed;

    for (size_t i = 0; i < len; ++i)
        h = 31 * h + p[i].unicode();

    return h;
}

size_t iHashFunc::operator()(const iChar& key) const
{
    return std::hash<ushort>()(key.unicode());
}

size_t iHashFunc::operator()(const iByteArray& key) const
{
    return hash_internal(key.constData(), size_t(key.size()), 0);
}

size_t iHashFunc::operator()(const iString& key) const
{
    return hash_internal(key.unicode(), size_t(key.size()), 0);
}

size_t iHashFunc::operator()(const iStringRef& key) const
{
    return hash_internal(key.unicode(), size_t(key.size()), 0);
}

size_t iHashFunc::operator()(const iStringView& key) const
{
    return hash_internal(key.data(), size_t(key.size()), 0);
}

size_t iHashFunc::operator()(const iLatin1String& key) const
{
    return hash_internal(key.data(), size_t(key.size()), 0);
}

bool iHashFunc::operator()(const iChar& key1, const iChar& key2) const
{
    return key1 == key2;
}

bool iHashFunc::operator()(const iByteArray& key1, const iByteArray& key2) const
{
    return key1 == key2;
}

bool iHashFunc::operator()(const iString& key1, const iString& key2) const
{
    return key1 == key2;
}

bool iHashFunc::operator()(const iStringRef& key1, const iStringRef& key2) const
{
    return key1 == key2;
}

bool iHashFunc::operator()(const iStringView& key1, const iStringView& key2) const
{
    return key1 == key2;
}

bool iHashFunc::operator()(const iLatin1String& key1, const iLatin1String& key2) const
{
    return key1 == key2;
}

} // namespace iShell
