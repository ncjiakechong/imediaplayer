/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibytearrayalgorithms.h
/// @brief   provides a view on an array of bytes with a read-only subset
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IBYTEARRAYALGORITHMS_H
#define IBYTEARRAYALGORITHMS_H

#include <string.h>
#include <stdarg.h>

#include <core/global/iglobal.h>
#include <core/global/inamespace.h>

namespace iShell {

class iByteArrayView;

namespace iPrivate {

IX_CORE_EXPORT bool startsWith(iByteArrayView haystack, iByteArrayView needle);

IX_CORE_EXPORT bool endsWith(iByteArrayView haystack, iByteArrayView needle);

IX_CORE_EXPORT xsizetype findByteArray(iByteArrayView haystack, xsizetype from, char needle);

IX_CORE_EXPORT xsizetype findByteArray(iByteArrayView haystack, xsizetype from, iByteArrayView needle);

IX_CORE_EXPORT xsizetype lastIndexOf(iByteArrayView haystack, xsizetype from, uchar needle);

IX_CORE_EXPORT xsizetype lastIndexOf(iByteArrayView haystack, xsizetype from, iByteArrayView needle);

IX_CORE_EXPORT xsizetype count(iByteArrayView haystack, iByteArrayView needle);

IX_CORE_EXPORT int compareMemory(iByteArrayView lhs, iByteArrayView rhs);

IX_CORE_EXPORT bool isValidUtf8(iByteArrayView s);

} // namespace iPrivate


/*****************************************************************************
  Safe and portable C string functions; extensions to standard cstring
 *****************************************************************************/

IX_CORE_EXPORT const void *imemrchr(const void *s, int needle, size_t n);

IX_CORE_EXPORT char *istrdup(const char *);

inline size_t istrlen(const char *str)
{ return str ? strlen(str) : 0; }

inline size_t istrnlen(const char *str, size_t maxlen)
{
    size_t length = 0;
    if (str) {
        while (length < maxlen && *str++)
            length++;
    }
    return length;
}

IX_CORE_EXPORT char *istrcpy(char *dst, const char *src);
IX_CORE_EXPORT char *istrncpy(char *dst, const char *src, size_t len);

IX_CORE_EXPORT int istrcmp(const char *str1, const char *str2);
IX_CORE_EXPORT int istrncmp(const char *, xsizetype, const char *, xsizetype = -1);

inline int istrncmp(const char *str1, const char *str2, size_t len)
{
    return (str1 && str2) ? strncmp(str1, str2, len)
        : (str1 ? 1 : (str2 ? -1 : 0));
}
IX_CORE_EXPORT int istricmp(const char *, const char *);
IX_CORE_EXPORT int istrnicmp(const char *, const char *, size_t len);
IX_CORE_EXPORT int istrnicmp(const char *, xsizetype, const char *, xsizetype = -1);

// iChecksum: Internet checksum
IX_CORE_EXPORT xuint16 iChecksum(const char *s, xsizetype len, iShell::ChecksumType standard); // ### Use iShell::ChecksumType standard = iShell::ChecksumIso3309

} // namespace iShell

#endif // IBYTEARRAYALGORITHMS_H