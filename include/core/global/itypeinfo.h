/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    itypeinfo.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ITYPEINFO_H
#define ITYPEINFO_H

#include <core/global/iglobal.h>

namespace iShell {

/*
   iTypeInfo     - type trait functionality
*/

template <typename T>
static constexpr bool iIsRelocatable()
{
    return std::is_enum<T>::value || std::is_integral<T>::value;
}

template <typename T>
static constexpr bool iIsTrivial()
{
    return std::is_enum<T>::value || std::is_integral<T>::value;
}

/*
  The catch-all template.
*/

template <typename T>
class iTypeInfo
{
public:
    enum {
        isSpecialized = std::is_enum<T>::value, // don't require every enum to be marked manually
        isPointer = false,
        isIntegral = std::is_integral<T>::value,
        isComplex = !iIsTrivial<T>(),
        isStatic = true,
        isRelocatable = iIsRelocatable<T>(),
        isLarge = (sizeof(T)>sizeof(void*)),
        sizeOf = sizeof(T)
    };
};

template<>
class iTypeInfo<void>
{
public:
    enum {
        isSpecialized = true,
        isPointer = false,
        isIntegral = false,
        isComplex = false,
        isStatic = false,
        isRelocatable = false,
        isLarge = false,
        sizeOf = 0
    };
};

template <typename T>
class iTypeInfo<T*>
{
public:
    enum {
        isSpecialized = true,
        isPointer = true,
        isIntegral = false,
        isComplex = false,
        isStatic = false,
        isRelocatable = true,
        isLarge = false,
        sizeOf = sizeof(T*)
    };
};

/*!
    \class iTypeInfoQuery
    \inmodule QtCore
    \internal
    \brief iTypeInfoQuery is used to query the values of a given iTypeInfo<T>

    We use it because there may be some iTypeInfo<T> specializations in user
    code that don't provide certain flags that we added after Qt 5.0. They are:
    \list
      \li isRelocatable: defaults to !isStatic
    \endlist

    DO NOT specialize this class elsewhere.
*/
// apply defaults for a generic iTypeInfo<T> that didn't provide the new values
template <typename T, typename = void>
struct iTypeInfoQuery : public iTypeInfo<T>
{
    enum { isRelocatable = !iTypeInfo<T>::isStatic };
};

// if iTypeInfo<T>::isRelocatable exists, use it
template <typename T>
struct iTypeInfoQuery<T, typename std::enable_if<iTypeInfo<T>::isRelocatable || true>::type> : public iTypeInfo<T>
{};

/*!
    \class iTypeInfoMerger
    \inmodule QtCore
    \internal

    \brief iTypeInfoMerger merges the iTypeInfo flags of T1, T2... and presents them
    as a iTypeInfo<T> would do.

    Let's assume that we have a simple set of structs:

    \snippet code/src_corelib_global_qglobal.cpp 50

    To create a proper iTypeInfo specialization for A struct, we have to check
    all sub-components; B, C and D, then take the lowest common denominator and call
    IX_DECLARE_TYPEINFO with the resulting flags. An easier and less fragile approach is to
    use iTypeInfoMerger, which does that automatically. So struct A would have
    the following iTypeInfo definition:

    \snippet code/src_corelib_global_qglobal.cpp 51
*/
template <class T, class T1, class T2 = T1, class T3 = T1, class T4 = T1>
class iTypeInfoMerger
{
public:
    enum {
        isSpecialized = true,
        isComplex = iTypeInfoQuery<T1>::isComplex || iTypeInfoQuery<T2>::isComplex
                    || iTypeInfoQuery<T3>::isComplex || iTypeInfoQuery<T4>::isComplex,
        isStatic = iTypeInfoQuery<T1>::isStatic || iTypeInfoQuery<T2>::isStatic
                    || iTypeInfoQuery<T3>::isStatic || iTypeInfoQuery<T4>::isStatic,
        isRelocatable = iTypeInfoQuery<T1>::isRelocatable && iTypeInfoQuery<T2>::isRelocatable
                    && iTypeInfoQuery<T3>::isRelocatable && iTypeInfoQuery<T4>::isRelocatable,
        isLarge = sizeof(T) > sizeof(void*),
        isPointer = false,
        isIntegral = false,
        sizeOf = sizeof(T)
    };
};

/*
   Specialize a specific type with:

     IX_DECLARE_TYPEINFO(type, flags);

   where 'type' is the name of the type to specialize and 'flags' is
   logically-OR'ed combination of the flags below.
*/
enum { /* TYPEINFO flags */
    IX_COMPLEX_TYPE = 0,
    IX_PRIMITIVE_TYPE = 0x1,
    IX_STATIC_TYPE = 0,
    IX_MOVABLE_TYPE = 0x2,               // ### Qt6: merge movable and relocatable once std::list no longer depends on it
    IX_DUMMY_TYPE = 0x4,
    IX_RELOCATABLE_TYPE = 0x8
};

#define IX_DECLARE_TYPEINFO_BODY(TYPE, FLAGS) \
class iTypeInfo<TYPE > \
{ \
public: \
    enum { \
        isSpecialized = true, \
        isComplex = (((FLAGS) & IX_PRIMITIVE_TYPE) == 0) && !iIsTrivial<TYPE>(), \
        isStatic = (((FLAGS) & (IX_MOVABLE_TYPE | IX_PRIMITIVE_TYPE)) == 0), \
        isRelocatable = !isStatic || ((FLAGS) & IX_RELOCATABLE_TYPE) || iIsRelocatable<TYPE>(), \
        isLarge = (sizeof(TYPE)>sizeof(void*)), \
        isPointer = false, \
        isIntegral = std::is_integral< TYPE >::value, \
        sizeOf = sizeof(TYPE) \
    }; \
    static inline const char *name() { return #TYPE; } \
}

#define IX_DECLARE_TYPEINFO(TYPE, FLAGS) \
template<> \
IX_DECLARE_TYPEINFO_BODY(TYPE, FLAGS)


/*
   Specialize a shared type with:

     IX_DECLARE_SHARED(type)

   where 'type' is the name of the type to specialize.  NOTE: shared
   types must define a member-swap, and be defined in the same
   namespace as iShell for this to work.
*/

#define IX_DECLARE_SHARED_IMPL(TYPE, FLAGS) \
IX_DECLARE_TYPEINFO(TYPE, FLAGS); \
inline void swap(TYPE &value1, TYPE &value2) \
{ value1.swap(value2); }
#define IX_DECLARE_SHARED(TYPE) IX_DECLARE_SHARED_IMPL(TYPE, IX_MOVABLE_TYPE)

/*
   iTypeInfo primitive specializations
*/
IX_DECLARE_TYPEINFO(bool, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(char, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(signed char, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(uchar, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(short, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(ushort, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(int, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(uint, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(long, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(ulong, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(float, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(double, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(long double, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(char16_t, IX_PRIMITIVE_TYPE);
IX_DECLARE_TYPEINFO(char32_t, IX_PRIMITIVE_TYPE);

} // namespace iShell


#endif // ITYPEINFO_H
