/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincoperation.cpp
/// @brief   async operation of INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <inc/iinccontext.h>
#include <inc/iincstream.h>
#include <inc/iincoperation.h>

namespace iShell {

iINCOperation::iINCOperation(iINCContext* c, iINCStream* s)
    : m_ref(1)
    , m_context(c)
    , m_stream(s)
    , m_state(STATE_RUNNING) 
{
    IX_ASSERT(c);

    /* Refcounting is strictly one-way: from the "bigger" to the "smaller" object. */
    c->m_operations.insert(this);
    ++m_ref;
}

iINCOperation::~iINCOperation()
{}

void iINCOperation::ref()
{
    IX_ASSERT(m_ref >= 1);
    ++m_ref;
}

void iINCOperation::deref() 
{
    IX_ASSERT(m_ref >= 1);

    if (--m_ref <= 0) {
        IX_ASSERT(!m_context);
        IX_ASSERT(!m_stream);

        delete this;
    }
}

void iINCOperation::unlink() 
{
    if (m_context) {
        IX_ASSERT(m_ref >= 2);

        std::unordered_set<iINCOperation*>::const_iterator it = m_context->m_operations.find(this);
        if (it != m_context->m_operations.cend())
            m_context->m_operations.erase(it);

        deref();
        m_context = IX_NULLPTR;
    }

    m_stream = IX_NULLPTR;
}

void iINCOperation::setState(State now) 
{
    IX_ASSERT(m_ref >= 1);

    if ((now == m_state) || (m_state == STATE_DONE) || (m_state == STATE_CANCELLED))
        return;

    ref();

    State pre = m_state;
    m_state = now;
    stateChanges(now, pre);

    if ((m_state == STATE_DONE) || (m_state== STATE_CANCELLED))
        unlink();

    deref();
}

} // namespace iShell
