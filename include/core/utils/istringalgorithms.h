/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringalgorithms.h
/// @brief   defines a collection of utility functions for performing various operations on strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGALGORITHMS_H
#define ISTRINGALGORITHMS_H

#include <vector>

#include <core/global/iglobal.h>
#include <core/global/inamespace.h>

namespace iShell {

class iByteArray;
class iLatin1StringView;
class iStringView;

namespace iPrivate {

IX_CORE_EXPORT xsizetype xustrlen(const xuint16 *str);
IX_CORE_EXPORT const xuint16 *xustrchr(iStringView str, xuint16 ch);
IX_CORE_EXPORT const xuint16 *xustrchr(iStringView str, xuint16 ch);

IX_CORE_EXPORT int compareStrings(iStringView   lhs, iStringView   rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT int compareStrings(iStringView   lhs, iLatin1StringView rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT int compareStrings(iLatin1StringView lhs, iStringView   rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT int compareStrings(iLatin1StringView lhs, iLatin1StringView rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);

IX_CORE_EXPORT bool startsWith(iStringView   haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT bool startsWith(iStringView   haystack, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT bool startsWith(iLatin1StringView haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT bool startsWith(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

IX_CORE_EXPORT bool endsWith(iStringView   haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT bool endsWith(iStringView   haystack, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT bool endsWith(iLatin1StringView haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT bool endsWith(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

IX_CORE_EXPORT xsizetype findString(iStringView str, xsizetype from, iChar needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype findString(iStringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype findString(iStringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype findString(iLatin1StringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype findString(iLatin1StringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

IX_CORE_EXPORT xsizetype lastIndexOf(iStringView haystack, xsizetype from, xuint16 needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype lastIndexOf(iStringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype lastIndexOf(iStringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype lastIndexOf(iLatin1StringView haystack, xsizetype from, iStringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype lastIndexOf(iLatin1StringView haystack, xsizetype from, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

IX_CORE_EXPORT iStringView   trimmed(iStringView   s);
IX_CORE_EXPORT iLatin1StringView trimmed(iLatin1StringView s);

IX_CORE_EXPORT xsizetype count(iStringView haystack, iChar needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype count(iStringView haystack, iStringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype count(iStringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype count(iLatin1StringView haystack, iLatin1StringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype count(iLatin1StringView haystack, iStringView needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
IX_CORE_EXPORT xsizetype count(iLatin1StringView haystack, iChar needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

IX_CORE_EXPORT bool isLower(iStringView s);
IX_CORE_EXPORT bool isUpper(iStringView s);

IX_CORE_EXPORT iByteArray convertToUtf8(iLatin1StringView string);
IX_CORE_EXPORT iByteArray convertToLatin1(iStringView str);
IX_CORE_EXPORT iByteArray convertToUtf8(iStringView str);
IX_CORE_EXPORT iByteArray convertToLocal8Bit(iStringView str);
IX_CORE_EXPORT std::list<xuint32> convertToUcs4(iStringView str);
IX_CORE_EXPORT bool isRightToLeft(iStringView string);

IX_CORE_EXPORT bool isAscii(iLatin1StringView s);
IX_CORE_EXPORT bool isAscii(iStringView   s);
IX_CORE_EXPORT bool isLatin1(iLatin1StringView s);
IX_CORE_EXPORT bool isLatin1(iStringView   s);
IX_CORE_EXPORT bool isValidUtf16(iStringView s);

} // namespace iPrivate

} // namespace iShell

#endif // ISTRINGALGORITHMS_H
