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
    : iObject(c)
    , m_context(c)
    , m_stream(s)
    , m_state(STATE_RUNNING) 
{
    IX_ASSERT(c);

    /* Refcounting is strictly one-way: from the "bigger" to the "smaller" object. */
    c->m_operations.insert(this);
}

iINCOperation::~iINCOperation()
{
    if (STATE_RUNNING == m_state) {
        m_state = STATE_CANCELLED;
        unlink();
    }
}

void iINCOperation::unlink() 
{
    if (m_context) {
        std::unordered_set<iINCOperation*>::const_iterator it = m_context->m_operations.find(this);
        if (it != m_context->m_operations.cend())
            m_context->m_operations.erase(it);

        m_context = IX_NULLPTR;
    }

    m_stream = IX_NULLPTR;
}

void iINCOperation::setState(State now) 
{
    if ((now == m_state) || (STATE_RUNNING != m_state))
        return;

    State pre = m_state;
    m_state = now;
    IEMIT stateChanged(now, pre);

    if (STATE_RUNNING != m_state)
        unlink();
}

} // namespace iShell
