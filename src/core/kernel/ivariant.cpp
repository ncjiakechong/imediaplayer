/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ivariant.cpp
/// @brief   provides a generic container that can hold values of different types
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <map>
#include <cstring>
#if __cplusplus >= 201103L
#include <unordered_map>
#endif

#include "core/io/ilog.h"
#include "core/kernel/ivariant.h"
#include "core/utils/istring.h"
#include "core/global/iglobalstatic.h"
#include "utils/ibasicatomicbitfield.h"

#define ILOG_TAG "ix_core"

namespace iShell {

typedef iBasicAtomicBitField<4096> iTypeIdContainer;
#if __cplusplus >= 201103L
typedef std::unordered_map< int, iVariant::iTypeHandler> iMetaTypeHandler;
typedef std::unordered_map< iLatin1StringView, int, iKeyHashFunc > iTypeIdRegister;
typedef std::unordered_map< std::pair<int, int>, const iAbstractConverterFunction*, iKeyHashFunc > iMetaTypeConverter;
#else
typedef std::map< int, iVariant::iTypeHandler> iMetaTypeHandler;
typedef std::map< iLatin1StringView, int > iTypeIdRegister;
typedef std::map< std::pair<int, int>, const iAbstractConverterFunction* > iMetaTypeConverter;
#endif

struct _iMetaType {
    iMutex           _lock;
    iTypeIdContainer _typeIdContainer;
    iTypeIdRegister  _typeIdRegister;
    iMetaTypeHandler  _metaTypeHandler;
    iMetaTypeConverter _metaTypeConverter;

    // Reserve slot 0 so that allocateNext() always returns rawId >= 1.
    // This lets iVariant encode SOO as m_typeId = -rawId (never 0, which means null).
    _iMetaType() { _typeIdContainer.allocateSpecific(0); }

    static void initSystemConvert();
};

IX_GLOBAL_STATIC(_iMetaType, _iMetaTypeDef)

// Look up the registered handler for a raw type id. The type must already be registered.
static iVariant::iTypeHandler sooHandler(int rawId)
{
    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    iMetaTypeHandler::const_iterator it = _iMetaTypeDef->_metaTypeHandler.find(rawId);
    IX_ASSERT(it != _iMetaTypeDef->_metaTypeHandler.end());
    return it->second;
}

iAbstractConverterFunction::~iAbstractConverterFunction()
{ iVariant::unregisterConverterFunction(fromTypeId, toTypeId); }

bool iAbstractConverterFunction::registerTo() const
{ return iVariant::registerConverterFunction(this, fromTypeId, toTypeId); }

int iVariant::iRegisterMetaType(const char *type, const iTypeHandler& handler, int hint)
{
    bool needInitSystemConvert = !_iMetaTypeDef.exists();

    do {
        iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
        iTypeIdRegister::iterator it = _iMetaTypeDef->_typeIdRegister.find(iLatin1StringView(type));
        if (it != _iMetaTypeDef->_typeIdRegister.end())
            return it->second;

        if ((hint > 0)
            && (hint < iTypeIdContainer::NumBits)
            && _iMetaTypeDef->_typeIdContainer.allocateSpecific(hint)) {
            _iMetaTypeDef->_metaTypeHandler.insert(std::pair<int, iVariant::iTypeHandler>(hint, handler));
            _iMetaTypeDef->_typeIdRegister.insert(std::pair< iLatin1StringView, int >(iLatin1StringView(type), hint));
            break;
        }

        hint = _iMetaTypeDef->_typeIdContainer.allocateNext();
        _iMetaTypeDef->_metaTypeHandler.insert(std::pair<int, iVariant::iTypeHandler>(hint, handler));
        _iMetaTypeDef->_typeIdRegister.insert(std::pair< iLatin1StringView, int >(iLatin1StringView(type), hint));
    } while(false);

    if (needInitSystemConvert)
        _iMetaType::initSystemConvert();

    if (hint < 0)
        ilog_warn(type, " regist fail!!!, container(", iTypeIdContainer::NumBits,") maybe full");

    return hint;
}

bool iVariant::registerConverterFunction(const iAbstractConverterFunction *f, int from, int to)
{
    IX_ASSERT(_iMetaTypeDef.exists());
    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    std::pair<iMetaTypeConverter::iterator,bool> ret;
    ret = _iMetaTypeDef->_metaTypeConverter.insert(
                std::pair<std::pair<int, int>, const iAbstractConverterFunction*>(std::pair<int, int>(from, to), f));

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

iVariant::iAbstractVariantImpl::iAbstractVariantImpl(void* ptr, ImplFn impl)
    : _data(ptr), _impl(impl)
{}

iVariant::iAbstractVariantImpl::~iAbstractVariantImpl()
{}

void iVariant::iAbstractVariantImpl::free()
{ _impl(Destroy, this, IX_NULLPTR); }

iVariant::iVariant()
    : m_typeId(0)
{ new (m_rawStore._buf) iSharedPtr<iAbstractVariantImpl>(); }

void iVariant::destroySooAt()
{
    IX_ASSERT(m_typeId < 0);
    sooHandler(-m_typeId).destroy(m_rawStore._buf);
}

iVariant::~iVariant()
{
    if (m_typeId < 0) {
        sooHandler(-m_typeId).destroy(m_rawStore._buf);
    } else {
        heapShared().~iSharedPtr<iAbstractVariantImpl>();
    }
}

iVariant::iVariant(const iVariant &other)
    : m_typeId(other.m_typeId)
{
    if (other.m_typeId < 0) {
        sooHandler(-other.m_typeId).copyConstruct(m_rawStore._buf, other.m_rawStore._buf);
    } else {
        new (m_rawStore._buf) iSharedPtr<iAbstractVariantImpl>(other.heapShared());
    }
}

iVariant& iVariant::operator=(const iVariant &other)
{
    if (&other == this)
        return *this;

    // Destroy current contents.
    if (m_typeId < 0) {
        sooHandler(-m_typeId).destroy(m_rawStore._buf);
    } else {
        heapShared().~iSharedPtr<iAbstractVariantImpl>();
    }

    m_typeId = other.m_typeId;

    // Copy-construct from other.
    if (other.m_typeId < 0) {
        sooHandler(-other.m_typeId).copyConstruct(m_rawStore._buf, other.m_rawStore._buf);
    } else {
        new (m_rawStore._buf) iSharedPtr<iAbstractVariantImpl>(other.heapShared());
    }

    return *this;
}

bool iVariant::isNull() const
{
    if (m_typeId == 0)
        return true;
    if (m_typeId < 0)
        return false;   // SOO: storage is always valid
    return heapShared().isNull();
}

void iVariant::clear()
{
    if (m_typeId < 0) {
        sooHandler(-m_typeId).destroy(m_rawStore._buf);
        new (m_rawStore._buf) iSharedPtr<iAbstractVariantImpl>();
        m_typeId = 0;
    } else {
        m_typeId = 0;
        heapShared().clear();
    }
}

bool iVariant::equal(const iVariant &other) const
{
    if (isNull() || other.isNull())
        return (isNull() && other.isNull());

    const int myType = type();  // real typeId: ≥1 for both SOO and heap when not null

    iVariant::iTypeHandler handler;
    {
        iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
        iMetaTypeHandler::iterator it = _iMetaTypeDef->_metaTypeHandler.find(myType);
        if (_iMetaTypeDef->_metaTypeHandler.end() == it)
            return false;

        handler = it->second;
    }

    void* selfData = dataPtr();

    if (other.type() == myType)
        return handler.equal(selfData, other.dataPtr());

    if (m_typeId < 0) {
        // SOO: use a stack buffer for the temporary.
        iVariantRawStorage tmpBuf;
        handler.defaultConstruct(tmpBuf._buf);
        const bool ok = other.convert(myType, tmpBuf._buf);
        const bool result = ok && handler.equal(selfData, tmpBuf._buf);
        handler.destroy(tmpBuf._buf);
        return result;
    }

    // Heap path: need a default-constructed temporary of our type for other to convert into.
    const iAbstractVariantImpl::ImplFn implFn = heapShared().data()->_impl;
    const size_t bufSize = reinterpret_cast<size_t>(implFn(iAbstractVariantImpl::BufferSize, IX_NULLPTR, IX_NULLPTR));

    // Use a stack buffer for types that fit within the limit to avoid a heap allocation.
    enum { kMaxStackImplSize = 128 };
    if (bufSize <= kMaxStackImplSize) {
        union { char buf[kMaxStackImplSize]; void* align; } stackStorage;
        iAbstractVariantImpl* tmp = implFn(iAbstractVariantImpl::Create, IX_NULLPTR, stackStorage.buf);
        const bool ok = other.convert(myType, tmp->_data);
        const bool result = ok && handler.equal(selfData, tmp->_data);
        implFn(iAbstractVariantImpl::Destroy, tmp, stackStorage.buf);
        return result;
    }

    // Large type (> kMaxStackImplSize): fall back to heap allocation.
    iAbstractVariantImpl* tmp = implFn(iAbstractVariantImpl::Create, IX_NULLPTR, IX_NULLPTR);
    iSharedPtr<iAbstractVariantImpl> tDataImpl(tmp, &iAbstractVariantImpl::free);
    if (!other.convert(myType, tDataImpl->_data))
        return false;

    return handler.equal(selfData, tDataImpl->_data);
}

bool iVariant::canConvert(int targetTypeId) const
{
    if (targetTypeId == type())
        return true;

    return convert(targetTypeId, IX_NULLPTR);
}

bool iVariant::convert(int t, void *result) const
{
    const int myType = type();
    IX_ASSERT(myType != t);

    const int charTypeId = iMetaTypeId<char>(0);
    const int ccharTypeId = iMetaTypeId<const char>(0);
    const int charXTypeId = iMetaTypeId<char*>(0);
    const int ccharXTypeId = iMetaTypeId<const char*>(0);
    const int stringTypeId = iMetaTypeId<std::string>(0);

    do {
        if (stringTypeId != t)
            break;

        std::string *str = static_cast<std::string *>(result);
        const void* ai = dataPtr();

        if (charTypeId == myType) {
            if (str) *str = *static_cast<const char*>(ai);
            return true;
        }

        if (ccharTypeId == myType) {
            if (str) *str = *static_cast<const char*>(ai);
            return true;
        }

        if (charXTypeId == myType) {
            if (str) *str = *static_cast<char* const*>(ai);
            return true;
        }

        if (ccharXTypeId == myType) {
            if (str) *str = *static_cast<const char* const*>(ai);
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
        const void* ai = dataPtr();

        if (wCharTypeId == myType) {
            if (str) *str = *static_cast<const wchar_t*>(ai);
            return true;
        }

        if (wcCharTypeId == myType) {
            if (str) *str = *static_cast<const wchar_t*>(ai);
            return true;
        }

        if (wCharXTypeId == myType) {
            if (str) *str = *static_cast<wchar_t* const*>(ai);
            return true;
        }

        if (wcCharXTypeId == myType) {
            if (str) *str = *static_cast<const wchar_t* const*>(ai);
            return true;
        }
    } while (0);

    const int iStringTypeId = iMetaTypeId<iString>(0);
    do {
        if (iStringTypeId != t)
            break;

        iString* str = static_cast<iString*>(result);
        const void* ai = dataPtr();

        if (charTypeId == myType) {
            if (str) *str = iChar(*static_cast<const char*>(ai));
            return true;
        }
        if (ccharTypeId == myType) {
            if (str) *str = iChar(*static_cast<const char*>(ai));
            return true;
        }
        if (charXTypeId == myType) {
            if (str) *str = iString::fromUtf8(iByteArrayView(*static_cast<char* const*>(ai)));
            return true;
        }
        if (ccharXTypeId == myType) {
            if (str) *str = iString::fromUtf8(iByteArrayView(*static_cast<const char* const*>(ai)));
            return true;
        }
        if (stringTypeId == myType) {
            if (str) *str = iString::fromStdString(*static_cast<const std::string*>(ai));
            return true;
        }
        if (wCharTypeId == myType) {
            if (str) *str = iChar(*static_cast<const wchar_t*>(ai));
            return true;
        }
        if (wcCharTypeId == myType) {
            if (str) *str = iChar(*static_cast<const wchar_t*>(ai));
            return true;
        }
        if (wCharXTypeId == myType) {
            if (str) *str = iString::fromWCharArray(*static_cast<wchar_t* const*>(ai));
            return true;
        }
        if (wStringTypeId == myType) {
            if (str) *str = iString::fromStdWString(*static_cast<const std::wstring*>(ai));
            return true;
        }

    } while (0);

    IX_ASSERT(_iMetaTypeDef.exists());
    iMetaTypeConverter::const_iterator it;
    iScopedLock<iMutex> _lock(_iMetaTypeDef->_lock);
    it = _iMetaTypeDef->_metaTypeConverter.find(std::pair<int, int>(myType, t));
    if (it == _iMetaTypeDef->_metaTypeConverter.end() || !it->second)
        return false;

    if (IX_NULLPTR == result)
        return true;

    return it->second->convert(it->second, dataPtr(), result);
}

/////////////////////////////////////////////////////////////////
/// system type convert implement
/////////////////////////////////////////////////////////////////
void _iMetaType::initSystemConvert()
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

} // namespace iShell
