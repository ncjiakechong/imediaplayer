/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#include <map>
#include "core/io/ilog.h"
#include "core/kernel/ivariant.h"
#include "core/utils/istring.h"
#include "core/global/iglobalstatic.h"
#include "utils/ibasicatomicbitfield.h"

#define ILOG_TAG "ix:core"

namespace iShell {

typedef iBasicAtomicBitField< std::numeric_limits<xint16>::max() > iTypeIdContainer;
typedef std::unordered_map< iLatin1String, int, iKeyHashFunc, iKeyEqualFunc > iTypeIdRegister;
typedef std::unordered_map< std::pair<int, int>, const iAbstractConverterFunction*, iKeyHashFunc, iKeyEqualFunc > iMetaTypeConverter;

struct _iMetaType {
    iMutex           _lock;
    iTypeIdContainer _typeIdContainer;
    iTypeIdRegister  _typeIdRegister;
    iMetaTypeConverter _metaTypeConverter;
};

IX_GLOBAL_STATIC(_iMetaType, _iMetaTypeDef)

iAbstractConverterFunction::~iAbstractConverterFunction()
{
    iVariant::unregisterConverterFunction(fromTypeId, toTypeId);
}

int iVariant::iRegisterMetaType(const char *type, int hint)
{
    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    iTypeIdRegister::iterator it = _iMetaTypeDef->_typeIdRegister.find(iLatin1String(type));
    if (it != _iMetaTypeDef->_typeIdRegister.end())
        return it->second;

    if ((hint > 0)
        && (hint < iTypeIdContainer::NumBits)
        && _iMetaTypeDef->_typeIdContainer.allocateSpecific(hint)) {
        _iMetaTypeDef->_typeIdRegister.insert(std::pair< iLatin1String, int >(iLatin1String(type), hint));
        return hint;
    }

    hint = _iMetaTypeDef->_typeIdContainer.allocateNext();
    _iMetaTypeDef->_typeIdRegister.insert(std::pair< iLatin1String, int >(iLatin1String(type), hint));
    return hint;
}

bool iVariant::registerConverterFunction(const iAbstractConverterFunction *f, int from, int to)
{
    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    std::pair<iMetaTypeConverter::iterator,bool> ret;
    ret = _iMetaTypeDef->_metaTypeConverter.insert(
                std::pair<std::pair<int, int>, const iAbstractConverterFunction*>
                          (std::pair<int, int>(from, to), f));

    if (!ret.second)
        ilog_warn(from, " -> ", to," vonverter has regiseted!!!");

    return ret.second;
}

void iVariant::unregisterConverterFunction(int from, int to)
{
    if (_iMetaTypeDef.isDestroyed())
        return;

    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    _iMetaTypeDef->_metaTypeConverter.erase(std::pair<int, int>(from, to));
}

iVariant::iAbstractVariantImpl::iAbstractVariantImpl(void* ptr)
    : data(ptr)
{
}

iVariant::iAbstractVariantImpl::~iAbstractVariantImpl()
{
}

iVariant::iVariant()
    : m_typeId(0)
{
}

iVariant::~iVariant()
{
}

iVariant::iVariant(const iVariant &other)
    : m_typeId(other.m_typeId)
    , m_dataImpl(other.m_dataImpl)
{
}

iVariant& iVariant::operator=(const iVariant &other)
{
    if (&other == this)
        return *this;

    m_typeId = other.m_typeId;
    m_dataImpl = other.m_dataImpl;
    return *this;
}

bool iVariant::isNull() const
{
    if ((0 != m_typeId) && !m_dataImpl.isNull())
        return false;

    return true;
}

void iVariant::clear()
{
    m_typeId = 0;
    m_dataImpl.clear();
}

bool iVariant::canConvert(int targetTypeId) const
{
    if (targetTypeId == m_typeId)
        return true;

    return convert(targetTypeId, IX_NULLPTR);
}

bool iVariant::convert(int t, void *result) const
{
    IX_ASSERT(m_typeId != t);

    const int charTypeId = iMetaTypeId<char>(0);
    const int ccharTypeId = iMetaTypeId<const char>(0);
    const int charXTypeId = iMetaTypeId<char*>(0);
    const int ccharXTypeId = iMetaTypeId<const char*>(0);
    const int stringTypeId = iMetaTypeId<std::string>(0);

    do {
        if (stringTypeId != t)
            break;

        std::string *str = static_cast<std::string *>(result);
        if (charTypeId == m_typeId) {
            iVariantImpl<char>* imp = static_cast< iVariantImpl<char>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (ccharTypeId == m_typeId) {
            iVariantImpl<const char>* imp = static_cast< iVariantImpl<const char>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (charXTypeId == m_typeId) {
            iVariantImpl<char*>* imp = static_cast< iVariantImpl<char*>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (ccharXTypeId == m_typeId) {
            iVariantImpl<const char*>* imp = static_cast< iVariantImpl<const char*>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }
    } while (0);

    const int wCharTypeId = iMetaTypeId<wchar_t>(0);
    const int wcCharTypeId = iMetaTypeId<const wchar_t>(0);
    const int wCharXTypeId = iMetaTypeId<wchar_t*>(0);
    const int wcCharXTypeId = iMetaTypeId<const wchar_t*>(0);
    const int wStringTypeId = iMetaTypeId<std::wstring>(0);

    do {
        if (wStringTypeId != t)
            break;

        std::wstring *str = static_cast<std::wstring *>(result);
        if (wCharTypeId == m_typeId) {
            iVariantImpl<wchar_t>* imp = static_cast< iVariantImpl<wchar_t>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (wcCharTypeId == m_typeId) {
            iVariantImpl<const wchar_t>* imp = static_cast< iVariantImpl<const wchar_t>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (wCharXTypeId == m_typeId) {
            iVariantImpl<wchar_t*>* imp = static_cast< iVariantImpl<wchar_t*>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;
            return true;
        }

        if (wcCharXTypeId == m_typeId) {
            iVariantImpl<const wchar_t*>* imp = static_cast< iVariantImpl<const wchar_t*>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (wcCharXTypeId == m_typeId) {
            iVariantImpl<const wchar_t*>* imp = static_cast< iVariantImpl<const wchar_t*>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }
    } while (0);

    const int iStringTypeId = iMetaTypeId<iString>(0);
    do {
        if (iStringTypeId != t)
            break;

        iString *str = static_cast<iString *>(result);
        if (charTypeId == m_typeId) {
            iVariantImpl<char>* imp = static_cast< iVariantImpl<char>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (ccharTypeId == m_typeId) {
            iVariantImpl<const char>* imp = static_cast< iVariantImpl<const char>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (charXTypeId == m_typeId) {
            iVariantImpl<char*>* imp = static_cast< iVariantImpl<char*>* >(m_dataImpl.data());
            if (str)
                *str = iString::fromUtf8(imp->mValue);

            return true;
        }

        if (ccharXTypeId == m_typeId) {
            iVariantImpl<const char*>* imp = static_cast< iVariantImpl<const char*>* >(m_dataImpl.data());
            if (str)
                *str = iString::fromUtf8(imp->mValue);

            return true;
        }

        if (stringTypeId == m_typeId) {
            iVariantImpl<std::string>* imp = static_cast< iVariantImpl<std::string>* >(m_dataImpl.data());
            if (str)
                *str = iString::fromStdString(imp->mValue);

            return true;
        }

        if (wCharTypeId == m_typeId) {
            iVariantImpl<wchar_t>* imp = static_cast< iVariantImpl<wchar_t>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (wcCharTypeId == m_typeId) {
            iVariantImpl<const wchar_t>* imp = static_cast< iVariantImpl<const wchar_t>* >(m_dataImpl.data());
            if (str)
                *str = imp->mValue;

            return true;
        }

        if (wCharXTypeId == m_typeId) {
            iVariantImpl<wchar_t*>* imp = static_cast< iVariantImpl<wchar_t*>* >(m_dataImpl.data());
            if (str)
                *str = iString::fromWCharArray(imp->mValue);

            return true;
        }

        if (wStringTypeId == m_typeId) {
            iVariantImpl<std::wstring>* imp = static_cast< iVariantImpl<std::wstring>* >(m_dataImpl.data());
            if (str)
                *str = iString::fromStdWString(imp->mValue);

            return true;
        }


    } while (0);

    iMetaTypeConverter::const_iterator it;
    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    it = _iMetaTypeDef->_metaTypeConverter.find(std::pair<int, int>(m_typeId, t));
    if (it == _iMetaTypeDef->_metaTypeConverter.end() || !it->second)
        return false;

    if (IX_NULLPTR == result)
        return true;

    return it->second->convert(it->second, m_dataImpl->data, result);
}


/////////////////////////////////////////////////////////////////
/// system type convert implement
/////////////////////////////////////////////////////////////////
struct systemConvertHelper
{
    systemConvertHelper()
    {
        iRegisterConverter<char, short>();
        iRegisterConverter<char, int>();
        iRegisterConverter<char, long>();
        iRegisterConverter<char, long long>();
        iRegisterConverter<char, float>();
        iRegisterConverter<char, double>();
        iRegisterConverter<char, unsigned char>();
        iRegisterConverter<char, unsigned short>();
        iRegisterConverter<char, unsigned int>();
        iRegisterConverter<char, unsigned long>();
        iRegisterConverter<char, unsigned long long>();

        iRegisterConverter<unsigned char, char>();
        iRegisterConverter<unsigned char, short>();
        iRegisterConverter<unsigned char, int>();
        iRegisterConverter<unsigned char, long>();
        iRegisterConverter<unsigned char, long long>();
        iRegisterConverter<unsigned char, float>();
        iRegisterConverter<unsigned char, double>();
        iRegisterConverter<unsigned char, unsigned short>();
        iRegisterConverter<unsigned char, unsigned int>();
        iRegisterConverter<unsigned char, unsigned long>();
        iRegisterConverter<unsigned char, unsigned long long>();

        iRegisterConverter<short, char>();
        iRegisterConverter<short, int>();
        iRegisterConverter<short, long>();
        iRegisterConverter<short, long long>();
        iRegisterConverter<short, float>();
        iRegisterConverter<short, double>();
        iRegisterConverter<short, unsigned char>();
        iRegisterConverter<short, unsigned short>();
        iRegisterConverter<short, unsigned int>();
        iRegisterConverter<short, unsigned long>();
        iRegisterConverter<short, unsigned long long>();

        iRegisterConverter<unsigned short, char>();
        iRegisterConverter<unsigned short, short>();
        iRegisterConverter<unsigned short, int>();
        iRegisterConverter<unsigned short, long>();
        iRegisterConverter<unsigned short, long long>();
        iRegisterConverter<unsigned short, float>();
        iRegisterConverter<unsigned short, double>();
        iRegisterConverter<unsigned short, unsigned char>();
        iRegisterConverter<unsigned short, unsigned int>();
        iRegisterConverter<unsigned short, unsigned long>();
        iRegisterConverter<unsigned short, unsigned long long>();

        iRegisterConverter<int, char>();
        iRegisterConverter<int, short>();
        iRegisterConverter<int, long>();
        iRegisterConverter<int, long long>();
        iRegisterConverter<int, float>();
        iRegisterConverter<int, double>();
        iRegisterConverter<int, unsigned char>();
        iRegisterConverter<int, unsigned short>();
        iRegisterConverter<int, unsigned int>();
        iRegisterConverter<int, unsigned long>();
        iRegisterConverter<int, unsigned long long>();

        iRegisterConverter<unsigned int, char>();
        iRegisterConverter<unsigned int, short>();
        iRegisterConverter<unsigned int, int>();
        iRegisterConverter<unsigned int, long>();
        iRegisterConverter<unsigned int, long long>();
        iRegisterConverter<unsigned int, float>();
        iRegisterConverter<unsigned int, double>();
        iRegisterConverter<unsigned int, unsigned char>();
        iRegisterConverter<unsigned int, unsigned short>();
        iRegisterConverter<unsigned int, unsigned long>();
        iRegisterConverter<unsigned int, unsigned long long>();

        iRegisterConverter<long, char>();
        iRegisterConverter<long, short>();
        iRegisterConverter<long, int>();
        iRegisterConverter<long, long long>();
        iRegisterConverter<long, float>();
        iRegisterConverter<long, double>();
        iRegisterConverter<long, unsigned char>();
        iRegisterConverter<long, unsigned short>();
        iRegisterConverter<long, unsigned int>();
        iRegisterConverter<long, unsigned long>();
        iRegisterConverter<long, unsigned long long>();

        iRegisterConverter<unsigned long, char>();
        iRegisterConverter<unsigned long, short>();
        iRegisterConverter<unsigned long, int>();
        iRegisterConverter<unsigned long, long>();
        iRegisterConverter<unsigned long, long long>();
        iRegisterConverter<unsigned long, float>();
        iRegisterConverter<unsigned long, double>();
        iRegisterConverter<unsigned long, unsigned char>();
        iRegisterConverter<unsigned long, unsigned short>();
        iRegisterConverter<unsigned long, unsigned int>();
        iRegisterConverter<unsigned long, unsigned long long>();

        iRegisterConverter<long long, char>();
        iRegisterConverter<long long, short>();
        iRegisterConverter<long long, int>();
        iRegisterConverter<long long, float>();
        iRegisterConverter<long long, double>();
        iRegisterConverter<long long, unsigned char>();
        iRegisterConverter<long long, unsigned short>();
        iRegisterConverter<long long, unsigned int>();
        iRegisterConverter<long long, unsigned long>();
        iRegisterConverter<long long, unsigned long long>();

        iRegisterConverter<unsigned long long, char>();
        iRegisterConverter<unsigned long long, short>();
        iRegisterConverter<unsigned long long, int>();
        iRegisterConverter<unsigned long long, long>();
        iRegisterConverter<unsigned long long, long long>();
        iRegisterConverter<unsigned long long, float>();
        iRegisterConverter<unsigned long long, double>();
        iRegisterConverter<unsigned long long, unsigned char>();
        iRegisterConverter<unsigned long long, unsigned short>();
        iRegisterConverter<unsigned long long, unsigned int>();
        iRegisterConverter<unsigned long long, unsigned long>();

        iRegisterConverter<float, char>();
        iRegisterConverter<float, short>();
        iRegisterConverter<float, int>();
        iRegisterConverter<float, long long>();
        iRegisterConverter<float, long>();
        iRegisterConverter<float, double>();
        iRegisterConverter<float, unsigned char>();
        iRegisterConverter<float, unsigned short>();
        iRegisterConverter<float, unsigned int>();
        iRegisterConverter<float, unsigned long>();
        iRegisterConverter<float, unsigned long long>();

        iRegisterConverter<double, char>();
        iRegisterConverter<double, short>();
        iRegisterConverter<double, int>();
        iRegisterConverter<double, long>();
        iRegisterConverter<double, long long>();
        iRegisterConverter<double, float>();
        iRegisterConverter<double, unsigned char>();
        iRegisterConverter<double, unsigned short>();
        iRegisterConverter<double, unsigned int>();
        iRegisterConverter<double, unsigned long>();
        iRegisterConverter<double, unsigned long long>();

        iRegisterConverter(&std::string::c_str);
        iRegisterConverter(&std::wstring::c_str);
        iRegisterConverter(&iString::utf16);
    }
};

static systemConvertHelper s_convertHelp;

} // namespace iShell
