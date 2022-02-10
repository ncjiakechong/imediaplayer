/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibitarray.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cstring>

#include <core/global/iendian.h>
#include <core/utils/ibitarray.h>
#include <core/utils/ialgorithms.h>

namespace iShell {

/*!
    \class iBitArray
    \brief The iBitArray class provides an array of bits.

    \ingroup tools
    \ingroup shared
    \reentrant

    A iBitArray is an array that gives access to individual bits and
    provides operators (\l{operator&()}{AND}, \l{operator|()}{OR},
    \l{operator^()}{XOR}, and \l{operator~()}{NOT}) that work on
    entire arrays of bits. It uses \l{implicit sharing} (copy-on-write)
    to reduce memory usage and to avoid the needless copying of data.

    The following code constructs a iBitArray containing 200 bits
    initialized to false (0):

    \snippet code/src_corelib_tools_qbitarray.cpp 0

    To initialize the bits to true, either pass \c true as second
    argument to the constructor, or call fill() later on.

    iBitArray uses 0-based indexes, just like C++ arrays. To access
    the bit at a particular index position, you can use operator[]().
    On non-const bit arrays, operator[]() returns a reference to a
    bit that can be used on the left side of an assignment. For
    example:

    \snippet code/src_corelib_tools_qbitarray.cpp 1

    For technical reasons, it is more efficient to use testBit() and
    setBit() to access bits in the array than operator[](). For
    example:

    \snippet code/src_corelib_tools_qbitarray.cpp 2

    iBitArray supports \c{&} (\l{operator&()}{AND}), \c{|}
    (\l{operator|()}{OR}), \c{^} (\l{operator^()}{XOR}),
    \c{~} (\l{operator~()}{NOT}), as well as
    \c{&=}, \c{|=}, and \c{^=}. These operators work in the same way
    as the built-in C++ bitwise operators of the same name. For
    example:

    \snippet code/src_corelib_tools_qbitarray.cpp 3

    For historical reasons, iBitArray distinguishes between a null
    bit array and an empty bit array. A \e null bit array is a bit
    array that is initialized using iBitArray's default constructor.
    An \e empty bit array is any bit array with size 0. A null bit
    array is always empty, but an empty bit array isn't necessarily
    null:

    \snippet code/src_corelib_tools_qbitarray.cpp 4

    All functions except isNull() treat null bit arrays the same as
    empty bit arrays; for example, iBitArray() compares equal to
    iBitArray(0). We recommend that you always use isEmpty() and
    avoid isNull().
*/

/*!
    \fn iBitArray::iBitArray(iBitArray &&other)

    Move-constructs a iBitArray instance, making it point at the same
    object that \a other was pointing to.


*/

/*! \fn iBitArray::iBitArray()

    Constructs an empty bit array.

    \sa isEmpty()
*/

/*
 * iBitArray construction note:
 *
 * We overallocate the byte array by 1 byte. The first user bit is at
 * d.data()[1]. On the extra first byte, we store the difference between the
 * number of bits in the byte array (including this byte) and the number of
 * bits in the bit array. Therefore, it's always a number between 8 and 15.
 *
 * This allows for fast calculation of the bit array size:
 *    inline int size() const { return (d.size() << 3) - *d.constData(); }
 *
 * Note: for an array of zero size, *d.constData() is the iByteArray implicit NUL.
 */

/*!
    Constructs a bit array containing \a size bits. The bits are
    initialized with \a value, which defaults to false (0).
*/
iBitArray::iBitArray(int size, bool value)
    : d(size <= 0 ? 0 : 1 + (size + 7)/8, iShell::Uninitialized)
{
    IX_ASSERT_X(size >= 0, "iBitArray::iBitArray Size must be greater than or equal to 0.");
    if (size <= 0)
        return;

    uchar* c = reinterpret_cast<uchar*>(d.data());
    memset(c + 1, value ? 0xff : 0, d.size() - 1);
    *c = d.size()*8 - size;
    if (value && size && size & 7)
        *(c+1+size/8) &= (1 << (size & 7)) - 1;
}

/*! \fn int iBitArray::size() const

    Returns the number of bits stored in the bit array.

    \sa resize()
*/

/*! \fn int iBitArray::count() const

    Same as size().
*/

/*!
    If \a on is true, this function returns the number of
    1-bits stored in the bit array; otherwise the number
    of 0-bits is returned.
*/
int iBitArray::count(bool on) const
{
    int numBits = 0;
    const xuint8 *bits = reinterpret_cast<const xuint8 *>(d.data()) + 1;

    // the loops below will try to read from *end
    // it's the iByteArray implicit NUL, so it will not change the bit count
    const xuint8 *const end = reinterpret_cast<const xuint8 *>(d.end());

    while (bits + 7 <= end) {
        xuint64 v = iFromUnaligned<xuint64>(bits);
        bits += 8;
        numBits += int(iPopulationCount(v));
    }
    if (bits + 3 <= end) {
        xuint32 v = iFromUnaligned<xuint32>(bits);
        bits += 4;
        numBits += int(iPopulationCount(v));
    }
    if (bits + 1 < end) {
        xuint16 v = iFromUnaligned<xuint16>(bits);
        bits += 2;
        numBits += int(iPopulationCount(v));
    }
    if (bits < end)
        numBits += int(iPopulationCount(bits[0]));

    return on ? numBits : size() - numBits;
}

/*!
    Resizes the bit array to \a size bits.

    If \a size is greater than the current size, the bit array is
    extended to make it \a size bits with the extra bits added to the
    end. The new bits are initialized to false (0).

    If \a size is less than the current size, bits are removed from
    the end.

    \sa size()
*/
void iBitArray::resize(int size)
{
    if (!size) {
        d.resize(0);
    } else {
        int s = d.size();
        d.resize(1 + (size+7)/8);
        uchar* c = reinterpret_cast<uchar*>(d.data());
        if (size > (s << 3))
            memset(c + s, 0, d.size() - s);
        else if (size & 7)
            *(c+1+size/8) &= (1 << (size & 7)) - 1;
        *c = d.size()*8 - size;
    }
}

/*! \fn bool iBitArray::isEmpty() const

    Returns \c true if this bit array has size 0; otherwise returns
    false.

    \sa size()
*/

/*! \fn bool iBitArray::isNull() const

    Returns \c true if this bit array is null; otherwise returns \c false.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 5

    iShell makes a distinction between null bit arrays and empty bit
    arrays for historical reasons. For most applications, what
    matters is whether or not a bit array contains any data,
    and this can be determined using isEmpty().

    \sa isEmpty()
*/

/*! \fn bool iBitArray::fill(bool value, int size = -1)

    Sets every bit in the bit array to \a value, returning true if successful;
    otherwise returns \c false. If \a size is different from -1 (the default),
    the bit array is resized to \a size beforehand.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 6

    \sa resize()
*/

/*!
    \overload

    Sets bits at index positions \a begin up to (but not including) \a end
    to \a value.

    \a begin must be a valid index position in the bit array
    (0 <= \a begin < size()).

    \a end must be either a valid index position or equal to size(), in
    which case the fill operation runs until the end of the array
    (0 <= \a end <= size()).

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 15
*/

void iBitArray::fill(bool value, int begin, int end)
{
    while (begin < end && begin & 0x7)
        setBit(begin++, value);
    int len = end - begin;
    if (len <= 0)
        return;
    int s = len & ~0x7;
    uchar *c = reinterpret_cast<uchar*>(d.data());
    memset(c + (begin >> 3) + 1, value ? 0xff : 0, s >> 3);
    begin += s;
    while (begin < end)
        setBit(begin++, value);
}

/*!
    \fn const char *iBitArray::bits() const


    Returns a pointer to a dense bit array for this iBitArray. Bits are counted
    upwards from the least significant bit in each byte. The the number of bits
    relevant in the last byte is given by \c{size() % 8}.

    \sa fromBits(), size()
 */

/*!


    Creates a iBitArray with the dense bit array located at \a data, with \a
    size bits. The byte array at \a data must be at least \a size / 8 (rounded up)
    bytes long.

    If \a size is not a multiple of 8, this function will include the lowest
    \a size % 8 bits from the last byte in \a data.

    \sa bits()
 */
iBitArray iBitArray::fromBits(const char *data, xsizetype size)
{
    iBitArray result;
    if (size == 0)
        return result;
    xsizetype nbytes = (size + 7) / 8;

    result.d = iByteArray(nbytes + 1, iShell::Uninitialized);
    char *bits = result.d.data();
    memcpy(bits + 1, data, nbytes);

    // clear any unused bits from the last byte
    if (size & 7)
        bits[nbytes] &= 0xffU >> (8 - (size & 7));

    *bits = result.d.size() * 8 - size;
    return result;
}

/*! \fn bool iBitArray::isDetached() const

    \internal
*/

/*! \fn void iBitArray::detach()

    \internal
*/

/*! \fn void iBitArray::clear()

    Clears the contents of the bit array and makes it empty.

    \sa resize(), isEmpty()
*/

/*! \fn void iBitArray::truncate(int pos)

    Truncates the bit array at index position \a pos.

    If \a pos is beyond the end of the array, nothing happens.

    \sa resize()
*/

/*! \fn bool iBitArray::toggleBit(int i)

    Inverts the value of the bit at index position \a i, returning the
    previous value of that bit as either true (if it was set) or false (if
    it was unset).

    If the previous value was 0, the new value will be 1. If the
    previous value was 1, the new value will be 0.

    \a i must be a valid index position in the bit array (i.e., 0 <=
    \a i < size()).

    \sa setBit(), clearBit()
*/

/*! \fn bool iBitArray::testBit(int i) const

    Returns \c true if the bit at index position \a i is 1; otherwise
    returns \c false.

    \a i must be a valid index position in the bit array (i.e., 0 <=
    \a i < size()).

    \sa setBit(), clearBit()
*/

/*! \fn bool iBitArray::setBit(int i)

    Sets the bit at index position \a i to 1.

    \a i must be a valid index position in the bit array (i.e., 0 <=
    \a i < size()).

    \sa clearBit(), toggleBit()
*/

/*! \fn void iBitArray::setBit(int i, bool value)

    \overload

    Sets the bit at index position \a i to \a value.
*/

/*! \fn void iBitArray::clearBit(int i)

    Sets the bit at index position \a i to 0.

    \a i must be a valid index position in the bit array (i.e., 0 <=
    \a i < size()).

    \sa setBit(), toggleBit()
*/

/*! \fn bool iBitArray::at(int i) const

    Returns the value of the bit at index position \a i.

    \a i must be a valid index position in the bit array (i.e., 0 <=
    \a i < size()).

    \sa operator[]()
*/

/*! \fn iBitRef iBitArray::operator[](int i)

    Returns the bit at index position \a i as a modifiable reference.

    \a i must be a valid index position in the bit array (i.e., 0 <=
    \a i < size()).

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 7

    The return value is of type iBitRef, a helper class for iBitArray.
    When you get an object of type iBitRef, you can assign to
    it, and the assignment will apply to the bit in the iBitArray
    from which you got the reference.

    The functions testBit(), setBit(), and clearBit() are slightly
    faster.

    \sa at(), testBit(), setBit(), clearBit()
*/

/*! \fn bool iBitArray::operator[](int i) const

    \overload
*/

/*! \fn iBitRef iBitArray::operator[](uint i)

    \overload
*/

/*! \fn bool iBitArray::operator[](uint i) const

    \overload
*/

/*! \fn iBitArray::iBitArray(const iBitArray &other)

    Constructs a copy of \a other.

    This operation takes \l{constant time}, because iBitArray is
    \l{implicitly shared}. This makes returning a iBitArray from a
    function very fast. If a shared instance is modified, it will be
    copied (copy-on-write), and that takes \l{linear time}.

    \sa operator=()
*/

/*! \fn iBitArray &iBitArray::operator=(const iBitArray &other)

    Assigns \a other to this bit array and returns a reference to
    this bit array.
*/

/*! \fn iBitArray &iBitArray::operator=(iBitArray &&other)


    Moves \a other to this bit array and returns a reference to
    this bit array.
*/

/*! \fn void iBitArray::swap(iBitArray &other)


    Swaps bit array \a other with this bit array. This operation is very
    fast and never fails.
*/

/*! \fn bool iBitArray::operator==(const iBitArray &other) const

    Returns \c true if \a other is equal to this bit array; otherwise
    returns \c false.

    \sa operator!=()
*/

/*! \fn bool iBitArray::operator!=(const iBitArray &other) const

    Returns \c true if \a other is not equal to this bit array;
    otherwise returns \c false.

    \sa operator==()
*/

/*!
    Performs the AND operation between all bits in this bit array and
    \a other. Assigns the result to this bit array, and returns a
    reference to it.

    The result has the length of the longest of the two bit arrays,
    with any missing bits (if one array is shorter than the other)
    taken to be 0.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 8

    \sa operator&(), operator|=(), operator^=(), operator~()
*/

iBitArray &iBitArray::operator&=(const iBitArray &other)
{
    resize(std::max(size(), other.size()));
    uchar *a1 = reinterpret_cast<uchar*>(d.data()) + 1;
    const uchar *a2 = reinterpret_cast<const uchar*>(other.d.constData()) + 1;
    int n = other.d.size() -1 ;
    int p = d.size() - 1 - n;
    while (n-- > 0)
        *a1++ &= *a2++;
    while (p-- > 0)
        *a1++ = 0;
    return *this;
}

/*!
    Performs the OR operation between all bits in this bit array and
    \a other. Assigns the result to this bit array, and returns a
    reference to it.

    The result has the length of the longest of the two bit arrays,
    with any missing bits (if one array is shorter than the other)
    taken to be 0.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 9

    \sa operator|(), operator&=(), operator^=(), operator~()
*/

iBitArray &iBitArray::operator|=(const iBitArray &other)
{
    resize(std::max(size(), other.size()));
    uchar *a1 = reinterpret_cast<uchar*>(d.data()) + 1;
    const uchar *a2 = reinterpret_cast<const uchar *>(other.d.constData()) + 1;
    int n = other.d.size() - 1;
    while (n-- > 0)
        *a1++ |= *a2++;
    return *this;
}

/*!
    Performs the XOR operation between all bits in this bit array and
    \a other. Assigns the result to this bit array, and returns a
    reference to it.

    The result has the length of the longest of the two bit arrays,
    with any missing bits (if one array is shorter than the other)
    taken to be 0.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 10

    \sa operator^(), operator&=(), operator|=(), operator~()
*/

iBitArray &iBitArray::operator^=(const iBitArray &other)
{
    resize(std::max(size(), other.size()));
    uchar *a1 = reinterpret_cast<uchar*>(d.data()) + 1;
    const uchar *a2 = reinterpret_cast<const uchar *>(other.d.constData()) + 1;
    int n = other.d.size() - 1;
    while (n-- > 0)
        *a1++ ^= *a2++;
    return *this;
}

/*!
    Returns a bit array that contains the inverted bits of this bit
    array.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 11

    \sa operator&(), operator|(), operator^()
*/

iBitArray iBitArray::operator~() const
{
    int sz = size();
    iBitArray a(sz);
    const uchar *a1 = reinterpret_cast<const uchar *>(d.constData()) + 1;
    uchar *a2 = reinterpret_cast<uchar*>(a.d.data()) + 1;
    int n = d.size() - 1;

    while (n-- > 0)
        *a2++ = ~*a1++;

    if (sz && sz%8)
        *(a2-1) &= (1 << (sz%8)) - 1;
    return a;
}

/*!
    \relates iBitArray

    Returns a bit array that is the AND of the bit arrays \a a1 and \a
    a2.

    The result has the length of the longest of the two bit arrays,
    with any missing bits (if one array is shorter than the other)
    taken to be 0.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 12

    \sa {iBitArray::}{operator&=()}, {iBitArray::}{operator|()}, {iBitArray::}{operator^()}
*/

iBitArray operator&(const iBitArray &a1, const iBitArray &a2)
{
    iBitArray tmp = a1;
    tmp &= a2;
    return tmp;
}

/*!
    \relates iBitArray

    Returns a bit array that is the OR of the bit arrays \a a1 and \a
    a2.

    The result has the length of the longest of the two bit arrays,
    with any missing bits (if one array is shorter than the other)
    taken to be 0.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 13

    \sa iBitArray::operator|=(), operator&(), operator^()
*/

iBitArray operator|(const iBitArray &a1, const iBitArray &a2)
{
    iBitArray tmp = a1;
    tmp |= a2;
    return tmp;
}

/*!
    \relates iBitArray

    Returns a bit array that is the XOR of the bit arrays \a a1 and \a
    a2.

    The result has the length of the longest of the two bit arrays,
    with any missing bits (if one array is shorter than the other)
    taken to be 0.

    Example:
    \snippet code/src_corelib_tools_qbitarray.cpp 14

    \sa {iBitArray}{operator^=()}, {iBitArray}{operator&()}, {iBitArray}{operator|()}
*/

iBitArray operator^(const iBitArray &a1, const iBitArray &a2)
{
    iBitArray tmp = a1;
    tmp ^= a2;
    return tmp;
}

/*!
    \class iBitRef
    \reentrant
    \brief The iBitRef class is an internal class, used with iBitArray.

    \internal

    The iBitRef is required by the indexing [] operator on bit arrays.
    It is not for use in any other context.
*/

/*! \fn iBitRef::iBitRef (iBitArray& a, int i)

    Constructs a reference to element \a i in the iBitArray \a a.
    This is what iBitArray::operator[] constructs its return value
    with.
*/

/*! \fn iBitRef::operator bool() const

    Returns the value referenced by the iBitRef.
*/

/*! \fn bool iBitRef::operator!() const

    \internal
*/

/*! \fn iBitRef& iBitRef::operator= (const iBitRef& v)

    Sets the value referenced by the iBitRef to that referenced by
    iBitRef \a v.
*/

/*! \fn iBitRef& iBitRef::operator= (bool v)
    \overload

    Sets the value referenced by the iBitRef to \a v.
*/

/*!
    \typedef iBitArray::DataPtr
    \internal
*/

} // namespace iShell
