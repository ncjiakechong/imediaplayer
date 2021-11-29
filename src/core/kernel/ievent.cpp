/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ievent.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <limits>
#include <algorithm>

#include "core/thread/iatomiccounter.h"
#include "core/kernel/ievent.h"
#include "utils/ibasicatomicbitfield.h"

namespace iShell {

typedef iBasicAtomicBitField<iEvent::MaxUser - iEvent::User + 1> UserEventTypeRegistry;

static UserEventTypeRegistry userEventTypeRegistry;

static inline int registerEventTypeZeroBased(int id)
{
    // if the type hint hasn't been registered yet, take it:
    if (id < UserEventTypeRegistry::NumBits && id >= 0 && userEventTypeRegistry.allocateSpecific(id))
        return id;

    // otherwise, ignore hint:
    return userEventTypeRegistry.allocateNext();
}

int iEvent::registerEventType(int hint)
{
    const int result = registerEventTypeZeroBased(iEvent::MaxUser - hint);
    return result < 0 ? -1 : iEvent::MaxUser - result ;
}

iEvent::iEvent(unsigned short type)
    : m_type(type), m_posted(false), m_accept(true)
{}

iEvent::iEvent(const iEvent &other)
    : m_type(other.m_type), m_posted(other.m_posted), m_accept(other.m_accept)
{
}

iEvent& iEvent::operator=(const iEvent &other)
{
    if (&other == this)
        return *this;

    m_type = other.m_type;
    m_posted = other.m_posted;
    m_accept = other.m_accept;
    return *this;
}

iEvent::~iEvent()
{
}

iTimerEvent::iTimerEvent(int timerId, xintptr u)
    : iEvent(Timer)
    , id(timerId)
    , userdata(u)
{
}
iTimerEvent::~iTimerEvent()
{
}

iChildEvent::iChildEvent(Type type, iObject *child)
    : iEvent(type)
    , c(child)
{
}

iChildEvent::~iChildEvent()
{
}

iDeferredDeleteEvent::iDeferredDeleteEvent()
    : iEvent(iEvent::DeferredDelete)
    , level(0)
{ }

/*!
    \internal
*/
iDeferredDeleteEvent::~iDeferredDeleteEvent()
{ }

} // namespace iShell
