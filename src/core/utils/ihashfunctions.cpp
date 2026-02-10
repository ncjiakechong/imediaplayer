/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ihashfunctions.cpp
/// @brief   defines hash functions for various classes,
///          enabling their use as keys in hash tables
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

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

size_t iKeyHashFunc::operator()(const iChar& key) const
{ return static_cast<size_t>(key.unicode()); }

size_t iKeyHashFunc::operator()(const iByteArray& key) const
{ return hash_internal(key.constData(), size_t(key.size()), 0); }

size_t iKeyHashFunc::operator()(const iString& key) const
{ return hash_internal(key.unicode(), size_t(key.size()), 0); }

size_t iKeyHashFunc::operator()(const iStringView& key) const
{ return hash_internal(key.data(), size_t(key.size()), 0); }

size_t iKeyHashFunc::operator()(const iLatin1StringView& key) const
{ return hash_internal(key.data(), size_t(key.size()), 0); }

template <typename T>
static size_t hash_combine(size_t seed, const T &t)
{ return seed ^ (static_cast<size_t>(t) + 0x9e3779b9 + (seed << 6) + (seed >> 2)); }

size_t iKeyHashFunc::operator()(const std::pair<int, int>& key) const
{
    size_t seed = 0;
    seed = hash_combine(seed, key.first);
    seed = hash_combine(seed, key.second);

    return seed;
}

} // namespace iShell
