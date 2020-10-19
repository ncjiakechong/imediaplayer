/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipcoperation.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <ipc/iipccontext.h>
#include <ipc/iipcstream.h>
#include <ipc/iipcoperation.h>

namespace iShell {

iIPCOperation::iIPCOperation(iIPCContext* c, iIPCStream* s, cb_wraper_t cb, void *userdata)
    : m_ref(1)
    , m_context(c)
    , m_stream(s)
    , m_state(STATE_RUNNING)
    , m_userdata(userdata)
    , m_callback(cb)
    , m_state_userdata(IX_NULLPTR)
    , m_state_callback(IX_NULLPTR)
    , m_private(IX_NULLPTR) {
    IX_ASSERT(c);

    /* Refcounting is strictly one-way: from the "bigger" to the "smaller" object. */
    c->m_operations.insert(this);
    ++m_ref;
}

void iIPCOperation::ref()
{
    IX_ASSERT(m_ref >= 1);
    ++m_ref;
}

void iIPCOperation::deref() {
    IX_ASSERT(m_ref >= 1);

    if (--m_ref <= 0) {
        IX_ASSERT(!m_context);
        IX_ASSERT(!m_stream);

        delete this;
    }
}

void iIPCOperation::unlink() {
    if (m_context) {
        IX_ASSERT(m_ref >= 2);

        std::unordered_set<iIPCOperation*>::const_iterator it = m_context->m_operations.find(this);
        if (it != m_context->m_operations.cend())
            m_context->m_operations.erase(it);

        deref();
        m_context = IX_NULLPTR;
    }

    m_stream = IX_NULLPTR;
    m_callback = IX_NULLPTR;
    m_userdata = IX_NULLPTR;
    m_state_callback = IX_NULLPTR;
    m_state_userdata = IX_NULLPTR;
}

void iIPCOperation::setState(State st) {
    IX_ASSERT(m_ref >= 1);

    if ((st == m_state) || (m_state == STATE_DONE) || (m_state == STATE_CANCELLED))
        return;

    ref();

    m_state = st;
    if (m_state_callback)
        m_state_callback(this, m_state_userdata);

    if ((m_state == STATE_DONE) || (m_state== STATE_CANCELLED))
        unlink();

    deref();
}

void iIPCOperation::cancel() {
    IX_ASSERT(m_ref >= 1);
    setState(STATE_CANCELLED);
}

void iIPCOperation::done() {
    IX_ASSERT(m_ref >= 1);
    setState(STATE_DONE);
}

iIPCOperation::State iIPCOperation::getState() const {
    IX_ASSERT(m_ref >= 1);
    return m_state;
}

void iIPCOperation::setStateCallback(notify_cb_t cb, void *userdata) {
    IX_ASSERT(m_ref >= 1);

    if ((m_state == STATE_DONE) || (m_state== STATE_CANCELLED))
        return;

    m_state_callback = cb;
    m_state_userdata = userdata;
}

} // namespace iShell
