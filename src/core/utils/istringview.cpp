/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringview.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "core/utils/istringview.h"

namespace iShell {

/*!
    \class iStringView
    \brief The iStringView class provides a unified view on UTF-16 strings with a read-only subset of the iString API.
    \reentrant
    \ingroup tools
    \ingroup string-processing

    A iStringView references a contiguous portion of a UTF-16 string it does
    not own. It acts as an interface type to all kinds of UTF-16 string,
    without the need to construct a iString first.

    The UTF-16 string may be represented as an array (or an array-compatible
    data-structure such as iString,
    std::basic_string, etc.) of iChar, \c xuint16, \c char16_t (on compilers that
    support C++11 Unicode strings) or (on platforms, such as Windows,
    where it is a 16-bit type) \c wchar_t.

    iStringView is designed as an interface type; its main use-case is
    as a function parameter type. When iStringViews are used as automatic
    variables or data members, care must be taken to ensure that the referenced
    string data (for example, owned by a iString) outlives the iStringView on all code paths,
    lest the string view ends up referencing deleted data.

    When used as an interface type, iStringView allows a single function to accept
    a wide variety of UTF-16 string data sources. One function accepting iStringView
    thus replaces three function overloads (taking iString, and
    \c{(const iChar*, int)}), while at the same time enabling even more string data
    sources to be passed to the function, such as \c{u"Hello World"}, a \c char16_t
    string literal.

    iStringViews should be passed by value, not by reference-to-const:
    \snippet code/src_corelib_tools_qstringview.cpp 0

    If you want to give your users maximum freedom in what strings they can pass
    to your function, accompany the iStringView overload with overloads for

    \list
        \li \e iChar: this overload can delegate to the iStringView version:
            \snippet code/src_corelib_tools_qstringview.cpp 1
            even though, for technical reasons, iStringView cannot provide a
            iChar constructor by itself.
        \li \e iString: if you store an unmodified copy of the string and thus would
            like to take advantage of iString's implicit sharing.
        \li iLatin1String: if you can implement the function without converting the
            iLatin1String to UTF-16 first; users expect a function overloaded on
            iLatin1String to perform strictly less memory allocations than the
            semantically equivalent call of the iStringView version, involving
            construction of a iString from the iLatin1String.
    \endlist

    iStringView can also be used as the return value of a function. If you call a
    function returning iStringView, take extra care to not keep the iStringView
    around longer than the function promises to keep the referenced string data alive.
    If in doubt, obtain a strong reference to the data by calling toString() to convert
    the iStringView into a iString.

    iStringView is a \e{Literal Type}, but since it stores data as \c{char16_t}, iteration
    is not \c (casts from \c{const char16_t*} to \c{const iChar*}, which is not
    allowed in \c functions). You can use an indexed loop and/or utf16() in
    \c contexts instead.

    \note We strongly discourage the use of std::list<iStringView>,
    because std::list is a very inefficient container for iStringViews (it would heap-allocate
    every element). Use std::vector (or std::vector) to hold iStringViews instead.

    \sa iString
*/

/*!
    \typedef iStringView::storage_type

    Alias for \c{char16_t} for non-Windows or if IX_COMPILER_UNICODE_STRINGS
    is defined. Otherwise, alias for \c{wchar_t}.
*/

/*!
    \typedef iStringView::value_type

    Alias for \c{const iChar}. Provided for compatibility with the STL.
*/

/*!
    \typedef iStringView::difference_type

    Alias for \c{std::ptrdiff_t}. Provided for compatibility with the STL.
*/

/*!
    \typedef iStringView::size_type

    Alias for xsizetype. Provided for compatibility with the STL.

    Unlike other iShell classes, iStringView uses xsizetype as its \c size_type, to allow
    accepting data from \c{std::basic_string} without truncation. The iShell API functions,
    for example length(), return \c int, while the STL-compatible functions, for example
    size(), return \c size_type.
*/

/*!
    \typedef iStringView::reference

    Alias for \c{value_type &}. Provided for compatibility with the STL.

    iStringView does not support mutable references, so this is the same
    as const_reference.
*/

/*!
    \typedef iStringView::const_reference

    Alias for \c{value_type &}. Provided for compatibility with the STL.
*/

/*!
    \typedef iStringView::pointer

    Alias for \c{value_type *}. Provided for compatibility with the STL.

    iStringView does not support mutable pointers, so this is the same
    as const_pointer.
*/

/*!
    \typedef iStringView::const_pointer

    Alias for \c{value_type *}. Provided for compatibility with the STL.
*/

/*!
    \typedef iStringView::iterator

    This typedef provides an STL-style const iterator for iStringView.

    iStringView does not support mutable iterators, so this is the same
    as const_iterator.

    \sa const_iterator, reverse_iterator
*/

/*!
    \typedef iStringView::const_iterator

    This typedef provides an STL-style const iterator for iStringView.

    \sa iterator, const_reverse_iterator
*/

/*!
    \typedef iStringView::reverse_iterator

    This typedef provides an STL-style const reverse iterator for iStringView.

    iStringView does not support mutable reverse iterators, so this is the
    same as const_reverse_iterator.

    \sa const_reverse_iterator, iterator
*/

/*!
    \typedef iStringView::const_reverse_iterator

    This typedef provides an STL-style const reverse iterator for iStringView.

    \sa reverse_iterator, const_iterator
*/

/*!
    \fn iStringView::iStringView()

    Constructs a null string view.

    \sa isNull()
*/

/*!
    \fn iStringView::iStringView(std::nullptr_t)

    Constructs a null string view.

    \sa isNull()
*/

/*!
    \fn  template <typename Char> iStringView::iStringView(const Char *str, xsizetype len)

    Constructs a string view on \a str with length \a len.

    The range \c{[str,len)} must remain valid for the lifetime of this string view object.

    Passing \IX_NULLPTR as \a str is safe if \a len is 0, too, and results in a null string view.

    The behavior is undefined if \a len is negative or, when positive, if \a str is \IX_NULLPTR.

    This constructor only participates in overload resolution if \c Char is a compatible
    character type. The compatible character types are: \c iChar, \c xuint16, \c char16_t and
    (on platforms, such as Windows, where it is a 16-bit type) \c wchar_t.
*/

/*!
    \fn template <typename Char> iStringView::iStringView(const Char *first, const Char *last)

    Constructs a string view on \a first with length (\a last - \a first).

    The range \c{[first,last)} must remain valid for the lifetime of
    this string view object.

    Passing \c \IX_NULLPTR as \a first is safe if \a last is \IX_NULLPTR, too,
    and results in a null string view.

    The behavior is undefined if \a last precedes \a first, or \a first
    is \IX_NULLPTR and \a last is not.

    This constructor only participates in overload resolution if \c Char
    is a compatible character type. The compatible character types
    are: \c iChar, \c xuint16, \c char16_t and (on platforms, such as
    Windows, where it is a 16-bit type) \c wchar_t.
*/

/*!
    \fn template <typename Char> iStringView::iStringView(const Char *str)

    Constructs a string view on \a str. The length is determined
    by scanning for the first \c{Char(0)}.

    \a str must remain valid for the lifetime of this string view object.

    Passing \IX_NULLPTR as \a str is safe and results in a null string view.

    This constructor only participates in overload resolution if \a
    str is not an array and if \c Char is a compatible character
    type. The compatible character types are: \c iChar, \c xuint16, \c
    char16_t and (on platforms, such as Windows, where it is a 16-bit
    type) \c wchar_t.
*/

/*!
    \fn template <typename Char, size_t N> iStringView::iStringView(const Char (&string)[N])

    Constructs a string view on the character string literal \a string.
    The length is set to \c{N-1}, excluding the trailing \{Char(0)}.
    If you need the full array, use the constructor from pointer and
    size instead:

    \snippet code/src_corelib_tools_qstringview.cpp 2

    \a string must remain valid for the lifetime of this string view
    object.

    This constructor only participates in overload resolution if \a
    string is an actual array and \c Char is a compatible character
    type. The compatible character types are: \c iChar, \c xuint16, \c
    char16_t and (on platforms, such as Windows, where it is a 16-bit
    type) \c wchar_t.
*/

/*!
    \fn iStringView::iStringView(const iString &str)

    Constructs a string view on \a str.

    \c{str.data()} must remain valid for the lifetime of this string view object.

    The string view will be null if and only if \c{str.isNull()}.
*/

/*!
    \fn template <typename StdBasicString> iStringView::iStringView(const StdBasicString &str)

    Constructs a string view on \a str. The length is taken from \c{str.size()}.

    \c{str.data()} must remain valid for the lifetime of this string view object.

    This constructor only participates in overload resolution if \c StdBasicString is an
    instantiation of \c std::basic_string with a compatible character type. The
    compatible character types are: \c iChar, \c xuint16, \c char16_t and
    (on platforms, such as Windows, where it is a 16-bit type) \c wchar_t.

    The string view will be empty if and only if \c{str.empty()}. It is unspecified
    whether this constructor can result in a null string view (\c{str.data()} would
    have to return \IX_NULLPTR for this).

    \sa isNull(), isEmpty()
*/

/*!
    \fn iString iStringView::toString() const

    Returns a deep copy of this string view's data as a iString.

    The return value will be the null iString if and only if this string view is null.

    \warning iStringView can store strings with more than 2\sup{30} characters
    while iString cannot. Calling this function on a string view for which size()
    returns a value greater than \c{INT_MAX / 2} constitutes undefined behavior.
*/

/*!
    \fn const iChar *iStringView::data() const

    Returns a const pointer to the first character in the string.

    \note The character array represented by the return value is \e not null-terminated.

    \sa begin(), end(), utf16()
*/

/*!
    \fn const storage_type *iStringView::utf16() const

    Returns a const pointer to the first character in the string.

    \c{storage_type} is \c{char16_t}.

    \note The character array represented by the return value is \e not null-terminated.

    \sa begin(), end(), data()
*/

/*!
    \fn iStringView::const_iterator iStringView::begin() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character in
    the string.

    This function is provided for STL compatibility.

    \sa end(), cbegin(), rbegin(), data()
*/

/*!
    \fn iStringView::const_iterator iStringView::cbegin() const

    Same as begin().

    This function is provided for STL compatibility.

    \sa cend(), begin(), crbegin(), data()
*/

/*!
    \fn iStringView::const_iterator iStringView::end() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    This function is provided for STL compatibility.

    \sa begin(), cend(), rend()
*/

/*! \fn iStringView::const_iterator iStringView::cend() const

    Same as end().

    This function is provided for STL compatibility.

    \sa cbegin(), end(), crend()
*/

/*!
    \fn iStringView::const_reverse_iterator iStringView::rbegin() const

    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    This function is provided for STL compatibility.

    \sa rend(), crbegin(), begin()
*/

/*!
    \fn iStringView::const_reverse_iterator iStringView::crbegin() const

    Same as rbegin().

    This function is provided for STL compatibility.

    \sa crend(), rbegin(), cbegin()
*/

/*!
    \fn iStringView::const_reverse_iterator iStringView::rend() const

    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to one past
    the last character in the string, in reverse order.

    This function is provided for STL compatibility.

    \sa rbegin(), crend(), end()
*/

/*!
    \fn iStringView::const_reverse_iterator iStringView::crend() const

    Same as rend().

    This function is provided for STL compatibility.

    \sa crbegin(), rend(), cend()
*/

/*!
    \fn bool iStringView::empty() const

    Returns whether this string view is empty - that is, whether \c{size() == 0}.

    This function is provided for STL compatibility.

    \sa isEmpty(), isNull(), size(), length()
*/

/*!
    \fn bool iStringView::isEmpty() const

    Returns whether this string view is empty - that is, whether \c{size() == 0}.

    This function is provided for compatibility with other iShell containers.

    \sa empty(), isNull(), size(), length()
*/

/*!
    \fn bool iStringView::isNull() const

    Returns whether this string view is null - that is, whether \c{data() == IX_NULLPTR}.

    This functions is provided for compatibility with other iShell containers.

    \sa empty(), isEmpty(), size(), length()
*/

/*!
    \fn xsizetype iStringView::size() const

    Returns the size of this string view, in UTF-16 code points (that is,
    surrogate pairs count as two for the purposes of this function, the same
    as in iString).

    \sa empty(), isEmpty(), isNull(), length()
*/

/*!
    \fn int iStringView::length() const

    Same as size(), except returns the result as an \c int.

    This function is provided for compatibility with other iShell containers.

    \warning iStringView can represent strings with more than 2\sup{31} characters.
    Calling this function on a string view for which size() returns a value greater
    than \c{INT_MAX} constitutes undefined behavior.

    \sa empty(), isEmpty(), isNull(), size()
*/

/*!
    \fn iChar iStringView::operator[](xsizetype n) const

    Returns the character at position \a n in this string view.

    The behavior is undefined if \a n is negative or not less than size().

    \sa at(), front(), back()
*/

/*!
    \fn iChar iStringView::at(xsizetype n) const

    Returns the character at position \a n in this string view.

    The behavior is undefined if \a n is negative or not less than size().

    \sa operator[](), front(), back()
*/

/*!
    \fn iChar iStringView::front() const

    Returns the first character in the string. Same as first().

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string view constitutes
    undefined behavior.

    \sa back(), first(), last()
*/

/*!
    \fn iChar iStringView::back() const

    Returns the last character in the string. Same as last().

    This function is provided for STL compatibility.

    \warning Calling this function on an empty string view constitutes
    undefined behavior.

    \sa front(), first(), last()
*/

/*!
    \fn iChar iStringView::first() const

    Returns the first character in the string. Same as front().

    This function is provided for compatibility with other iShell containers.

    \warning Calling this function on an empty string view constitutes
    undefined behavior.

    \sa front(), back(), last()
*/

/*!
    \fn iChar iStringView::last() const

    Returns the last character in the string. Same as back().

    This function is provided for compatibility with other iShell containers.

    \warning Calling this function on an empty string view constitutes
    undefined behavior.

    \sa back(), front(), first()
*/

/*!
    \fn iStringView iStringView::mid(xsizetype start) const

    Returns the substring starting at position \a start in this object,
    and extending to the end of the string.

    \note The behavior is undefined when \a start < 0 or \a start > size().

    \sa left(), right(), chopped(), chop(), truncate()
*/

/*!
    \fn iStringView iStringView::mid(xsizetype start, xsizetype length) const
    \overload

    Returns the substring of length \a length starting at position
    \a start in this object.

    \note The behavior is undefined when \a start < 0, \a length < 0,
    or \a start + \a length > size().

    \sa left(), right(), chopped(), chop(), truncate()
*/

/*!
    \fn iStringView iStringView::left(xsizetype length) const

    Returns the substring of length \a length starting at position
    0 in this object.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), right(), chopped(), chop(), truncate()
*/

/*!
    \fn iStringView iStringView::right(xsizetype length) const

    Returns the substring of length \a length starting at position
    size() - \a length in this object.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), chopped(), chop(), truncate()
*/

/*!
    \fn iStringView iStringView::chopped(xsizetype length) const

    Returns the substring of length size() - \a length starting at the
    beginning of this object.

    Same as \c{left(size() - length)}.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), right(), chop(), truncate()
*/

/*!
    \fn void iStringView::truncate(xsizetype length)

    Truncates this string view to length \a length.

    Same as \c{*this = left(length)}.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), right(), chopped(), chop()
*/

/*!
    \fn void iStringView::chop(xsizetype length)

    Truncates this string view by \a length characters.

    Same as \c{*this = left(size() - length)}.

    \note The behavior is undefined when \a length < 0 or \a length > size().

    \sa mid(), left(), right(), chopped(), truncate()
*/

/*!
    \fn iStringView iStringView::trimmed() const

    Strips leading and trailing whitespace and returns the result.

    Whitespace means any character for which iChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.
*/

/*!
    \fn int iStringView::compare(iStringView other, iShell::CaseSensitivity cs) const

    Compares this string-view with the \a other string-view and returns an
    integer less than, equal to, or greater than zero if this string-view
    is less than, equal to, or greater than the other string-view.

    If \a cs is iShell::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    \sa operator==(), operator<(), operator>()
*/

/*!
    \fn bool iStringView::startsWith(iStringView str, iShell::CaseSensitivity cs) const
    \fn bool iStringView::startsWith(iLatin1String l1, iShell::CaseSensitivity cs) const
    \fn bool iStringView::startsWith(iChar ch) const
    \fn bool iStringView::startsWith(iChar ch, iShell::CaseSensitivity cs) const

    Returns \c true if this string-view starts with string-view \a str,
    Latin-1 string \a l1, or character \a ch, respectively;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (the default), the search is case-sensitive;
    otherwise the search is case-insensitive.

    \sa endsWith()
*/

/*!
    \fn bool iStringView::endsWith(iStringView str, iShell::CaseSensitivity cs) const
    \fn bool iStringView::endsWith(iLatin1String l1, iShell::CaseSensitivity cs) const
    \fn bool iStringView::endsWith(iChar ch) const
    \fn bool iStringView::endsWith(iChar ch, iShell::CaseSensitivity cs) const

    Returns \c true if this string-view ends with string-view \a str,
    Latin-1 string \a l1, or character \a ch, respectively;
    otherwise returns \c false.

    If \a cs is iShell::CaseSensitive (the default), the search is case-sensitive;
    otherwise the search is case-insensitive.

    \sa startsWith()
*/

/*!
    \fn iByteArray iStringView::toLatin1() const

    Returns a Latin-1 representation of the string as a iByteArray.

    The behavior is undefined if the string contains non-Latin1 characters.

    \sa toUtf8(), toLocal8Bit(), iTextCodec
*/

/*!
    \fn iByteArray iStringView::toLocal8Bit() const

    Returns a local 8-bit representation of the string as a iByteArray.

    iTextCodec::codecForLocale() is used to perform the conversion from
    Unicode. If the locale's encoding could not be determined, this function
    does the same as toLatin1().

    The behavior is undefined if the string contains characters not
    supported by the locale's 8-bit encoding.

    \sa toLatin1(), toUtf8(), iTextCodec
*/

/*!
    \fn iByteArray iStringView::toUtf8() const

    Returns a UTF-8 representation of the string as a iByteArray.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like iString.

    \sa toLatin1(), toLocal8Bit(), iTextCodec
*/

/*!
    \fn std::vector<xuint32> iStringView::toUcs4() const

    Returns a UCS-4/UTF-32 representation of the string as a std::vector<xuint32>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode replacement character
    (iChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not 0-terminated.

    \sa toUtf8(), toLatin1(), toLocal8Bit(), iTextCodec
*/

/*!
    \fn template <typename iStringLike> iToStringViewIgnoringNull(const iStringLike &s);
    \internal

    Convert \a s to a iStringView ignoring \c{s.isNull()}.

    Returns a string-view that references \a{s}' data, but is never null.

    This is a faster way to convert a iString to a iStringView,
    if null iStrings can legitimately be treated as empty ones.

    \sa iString::isNull(), iStringView
*/

/*!
    \fn bool iStringView::isRightToLeft() const

    Returns \c true if the string is read right to left.

    \sa iString::isRightToLeft()
*/

} // namespace iShell
