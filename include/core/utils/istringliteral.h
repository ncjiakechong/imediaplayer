/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringliteral.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGLITERAL_H
#define ISTRINGLITERAL_H

#include <core/utils/iarraydata.h>


namespace iShell {

typedef iTypedArrayData<ushort> iStringData;

// all our supported compilers support Unicode string literals,
// But iStringLiteral only needs the core language feature, so just use u"" here unconditionally:

typedef char16_t xunicodechar;

// xunicodechar must typedef an integral type of size 2
IX_COMPILER_VERIFY(sizeof(xunicodechar) == 2);

#define IX_UNICODE_LITERAL(str) u"" str
#define iStringLiteral(str) \
    ([]() -> iString { \
        enum { Size = sizeof(IX_UNICODE_LITERAL(str))/2 - 1 }; \
        static const iStaticStringData<Size> xstring_literal = { \
            IX_STATIC_STRING_DATA_HEADER_INITIALIZER(Size), \
            IX_UNICODE_LITERAL(str) }; \
        iStringDataPtr holder = { xstring_literal.data_ptr() }; \
        const iString xstring_literal_temp(holder); \
        return xstring_literal_temp; \
    }()) \
    /**/

#define IX_STATIC_STRING_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, offset) \
{ {iAtomicCounter<int>(-1)}, size, 0, 0, offset } \
    /**/

#define IX_STATIC_STRING_DATA_HEADER_INITIALIZER(size) \
    IX_STATIC_STRING_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, sizeof(iStringData)) \
    /**/


#define iStringViewLiteral(str) iStringView(IX_UNICODE_LITERAL(str))


template <int N>
struct iStaticStringData
{
    iArrayData str;
    xunicodechar data[N + 1];

    iStringData *data_ptr() const
    {
        IX_ASSERT(str.ref.isStatic());
        return const_cast<iStringData *>(static_cast<const iStringData*>(&str));
    }
};

struct iStringDataPtr
{
    iStringData *ptr;
};

} // namespace iShell

#endif // ISTRINGLITERAL_H
