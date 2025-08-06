/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringalgorithms.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGALGORITHMS_H
#define ISTRINGALGORITHMS_H

#include <vector>

#include <core/global/iglobal.h>
#include <core/global/inamespace.h>

namespace iShell {

class iByteArray;
class iLatin1String;
class iStringView;

namespace iPrivate {

xsizetype xustrlen(const ushort *str);
const ushort *xustrchr(iStringView str, ushort ch) noexcept;

int compareStrings(iStringView   lhs, iStringView   rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);
int compareStrings(iStringView   lhs, iLatin1String rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);
int compareStrings(iLatin1String lhs, iStringView   rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);
int compareStrings(iLatin1String lhs, iLatin1String rhs, iShell::CaseSensitivity cs = iShell::CaseSensitive);


bool startsWith(iStringView   haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
bool startsWith(iStringView   haystack, iLatin1String needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
bool startsWith(iLatin1String haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
bool startsWith(iLatin1String haystack, iLatin1String needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

bool endsWith(iStringView   haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
bool endsWith(iStringView   haystack, iLatin1String needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
bool endsWith(iLatin1String haystack, iStringView   needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);
bool endsWith(iLatin1String haystack, iLatin1String needle, iShell::CaseSensitivity cs = iShell::CaseSensitive);

iStringView   trimmed(iStringView   s);
iLatin1String trimmed(iLatin1String s);

iByteArray convertToLatin1(iStringView str);
iByteArray convertToUtf8(iStringView str);
iByteArray convertToLocal8Bit(iStringView str);
std::vector<uint> convertToUcs4(iStringView str);
bool isRightToLeft(iStringView string);

bool isAscii(iLatin1String s);
bool isAscii(iStringView   s);
bool isLatin1(iLatin1String s);
bool isLatin1(iStringView   s);

} // namespace iPrivate

} // namespace iShell

#endif // ISTRINGALGORITHMS_H
