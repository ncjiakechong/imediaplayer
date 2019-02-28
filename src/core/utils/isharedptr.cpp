/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isharedptr.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-10-10
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-10-10              anfengce@        Create.
/////////////////////////////////////////////////////////////////
#include "core/utils/isharedptr.h"
#include "core/kernel/iobject.h"
#include "core/io/ilog.h"

#define ILOG_TAG "core"

namespace ishell {

namespace isharedpointer {

ExternalRefCountData::~ExternalRefCountData()
{
    i_assert(!_weakRef.value());
    i_assert(_strongRef.value() <= 0);
}

int ExternalRefCountData::strongUnref()
{
    int next = --_strongRef;
    if ((0 == next) && _objFree)
        _objFree(this);

    return next;
}

bool ExternalRefCountData::testAndSetStrong(int expectedValue, int newValue)
{
    return  _strongRef.testAndSet(expectedValue, newValue);
}

int ExternalRefCountData::weakUnref()
{
    int next = --_weakRef;
    if (next > 0)
        return next;

    if ((0 == next) && _extFree)
        _extFree(this);

    return next;
}

void ExternalRefCountData::setObjectShared(const iObject *obj, bool share)
{
    i_check_ptr(obj);
    if (!share)
        return;

    iObject* d = const_cast<iObject*>(obj);
    if (d->m_refCount.testAndSet(I_NULLPTR, this)) {
        weakRef();
    }
}

void ExternalRefCountData::checkObjectShared(const iObject *)
{
    if (_strongRef.value() < 0)
        ilog_warn("iSharedPtr: cannot create a iSharedPtr from a iObject-tracking iWeakPtr");
}

ExternalRefCountData* ExternalRefCountData::getAndTest(const iObject* obj, ExternalRefCountData* data)
{
    i_check_ptr(obj);

    ExternalRefCountData *that = obj->m_refCount.load();
    if (that) {
        return that;
    }

    // we can create the refcount data because it doesn't exist
    that = data;
    iObject* d = const_cast<iObject*>(obj);
    if (!d->m_refCount.testAndSet(I_NULLPTR, that)) {
        that = d->m_refCount.load();
    }

    return that;
}

} // namespace isharedpointer

} // namespace ishell
