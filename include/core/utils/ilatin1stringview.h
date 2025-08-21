/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilatin1stringview.h
/// @brief   provide functionalities for Latin-1 strings
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ILATIN1STRINGVIEW_H
#define ILATIN1STRINGVIEW_H

#include <string>
#include <iterator>
#include <cstdarg>
#include <vector>

#include <core/global/imacro.h>
#include <core/utils/ichar.h>
#include <core/utils/ibytearray.h>
#include <core/global/inamespace.h>
#include <core/utils/istringalgorithms.h>
#include <core/utils/istringview.h>

namespace iShell {

class iString;

class iLatin1StringView
{
public:
    inline iLatin1StringView() : m_size(0), m_data(IX_NULLPTR) {}
    inline explicit iLatin1StringView(const char *s) : m_size(s ? xsizetype(strlen(s)) : 0), m_data(s) {}
    explicit iLatin1StringView(const char *f, const char *l)
        : iLatin1StringView(f, xsizetype(l - f)) {}
    inline explicit iLatin1StringView(const char *s, xsizetype sz) : m_size(sz), m_data(s) {}
    inline explicit iLatin1StringView(const iByteArray &s) : m_size(xsizetype(istrnlen(s.constData(), s.size()))), m_data(s.constData()) {}

    const char *latin1() const { return m_data; }
    xsizetype size() const { return m_size; }
    const char *data() const { return m_data; }

    bool isNull() const { return !data(); }
    bool isEmpty() const { return !size(); }

    iLatin1Char at(xsizetype i) const
    { IX_ASSERT(i >= 0); IX_ASSERT(i < size()); return iLatin1Char(m_data[i]); }
    iLatin1Char operator[](xsizetype i) const { return at(i); }

    iLatin1Char front() const { return at(0); }
    iLatin1Char back() const { return at(size() - 1); }

    bool startsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::startsWith(*this, s, cs); }
    bool startsWith(iChar c) const
    { return !isEmpty() && front().unicode() == c.unicode(); }
    inline bool startsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::startsWith(*this, iStringView(&c, 1), cs); }

    bool endsWith(iStringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iLatin1StringView s, iShell::CaseSensitivity cs = iShell::CaseSensitive) const
    { return iPrivate::endsWith(*this, s, cs); }
    bool endsWith(iChar c) const
    { return !isEmpty() && back().unicode() == c.unicode(); }
    inline bool endsWith(iChar c, iShell::CaseSensitivity cs) const
    { return iPrivate::endsWith(*this, iStringView(&c, 1), cs); }

    using value_type = const char;
    using reference = value_type&;
    using const_reference = reference;
    using iterator = value_type*;
    using const_iterator = iterator;
    using difference_type = xsizetype; // violates Container concept requirements
    using size_type = xsizetype;       // violates Container concept requirements

    const_iterator begin() const { return data(); }
    const_iterator cbegin() const { return data(); }
    const_iterator end() const { return data() + size(); }
    const_iterator cend() const { return data() + size(); }

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = reverse_iterator;

    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    iLatin1StringView mid(xsizetype pos) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(pos <= size()); return iLatin1StringView(m_data + pos, m_size - pos); }
    iLatin1StringView mid(xsizetype pos, xsizetype n) const
    { IX_ASSERT(pos >= 0); IX_ASSERT(n >= 0); IX_ASSERT(pos + n <= size()); return iLatin1StringView(m_data + pos, n); }
    iLatin1StringView left(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1StringView(m_data, n); }
    iLatin1StringView right(xsizetype n) const
    { IX_ASSERT(n >= 0); IX_ASSERT(n <= size()); return iLatin1StringView(m_data + m_size - n, n); }

    iLatin1StringView sliced(xsizetype pos) const
    { verify(pos, 0); return iLatin1StringView(m_data + pos, m_size - pos); }
    iLatin1StringView sliced(xsizetype pos, xsizetype n) const
    { verify(pos, n); return iLatin1StringView(m_data + pos, n); }
    iLatin1StringView first(xsizetype n) const
    { verify(0, n); return sliced(0, n); }
    iLatin1StringView last(xsizetype n) const
    { verify(0, n); return sliced(size() - n, n); }
    iLatin1StringView chopped(xsizetype n) const
    { verify(0, n); return sliced(0, size() - n); }
    void chop(xsizetype n)
    { verify(0, n); m_size -= n; }
    void truncate(xsizetype n)
    { verify(0, n); m_size = n; }

    iLatin1StringView trimmed() const { return iPrivate::trimmed(*this); }

    inline bool operator==(const iString &s) const;
    inline bool operator>(const iString &s) const;
    inline bool operator<(const iString &s) const;
    inline bool operator!=(const iString &s) const { return !operator==(s); }
    inline bool operator>=(const iString &s) const { return !operator<(s); }
    inline bool operator<=(const iString &s) const { return !operator>(s); }

    inline bool operator==(const char *s) const;
    inline bool operator<(const char *s) const;
    inline bool operator>(const char *s) const;
    inline bool operator!=(const char *s) const { return !operator==(s); }
    inline bool operator<=(const char *s) const { return !operator>(s); }
    inline bool operator>=(const char *s) const { return !operator<(s); }

    inline bool operator==(const iByteArray &s) const;
    inline bool operator<(const iByteArray &s) const;
    inline bool operator>(const iByteArray &s) const;
    inline bool operator!=(const iByteArray &s) const { return !operator==(s); }
    inline bool operator<=(const iByteArray &s) const { return !operator>(s); }
    inline bool operator>=(const iByteArray &s) const { return !operator<(s); }

private:
    inline void verify(xsizetype pos, xsizetype n = 1) const
    {
        IX_ASSERT(pos >= 0);
        IX_ASSERT(pos <= size());
        IX_ASSERT(n >= 0);
        IX_ASSERT(n <= size() - pos);
    }

    xsizetype m_size;
    const char* m_data;
};
IX_DECLARE_TYPEINFO(iLatin1StringView, IX_MOVABLE_TYPE);

//
// iLatin1StringView inline implementations
//
inline bool iPrivate::isLatin1(iLatin1StringView)
{ return true; }

//
// iStringView members that require iLatin1StringView:
//
inline bool iStringView::startsWith(iLatin1StringView s, iShell::CaseSensitivity cs) const
{ return iPrivate::startsWith(*this, s, cs); }
inline bool iStringView::endsWith(iLatin1StringView s, iShell::CaseSensitivity cs) const
{ return iPrivate::endsWith(*this, s, cs); }

} // namespace iShell

#endif
