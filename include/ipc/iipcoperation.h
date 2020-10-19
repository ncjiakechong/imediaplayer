/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipcoperation.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IIPCOPERATION_H
#define IIPCOPERATION_H

#include <core/global/imacro.h>
#include <core/thread/iatomiccounter.h>
#include <ipc/iipcglobal.h>

namespace iShell {

class iIPCStream;
class iIPCContext;

class IX_IPC_EXPORT iIPCOperation
{
public:
    /** A callback for operation state changes */
    typedef void (*notify_cb_t) (iIPCOperation* o, void* userdata);

    /** The state of an operation */
    enum State {
        STATE_RUNNING,      /**< The operation is still running */
        STATE_DONE,         /**< The operation has completed */
        STATE_CANCELLED
        /**< The operation has been cancelled. Operations may get cancelled by the
         * application, or as a result of the context getting disconnected while the
         * operation is pending. */
    };

    /** Increase the reference count by one */
    void ref();

    /** Decrease the reference count by one */
    void deref();

    /** Cancel the operation. Beware! This will not necessarily cancel the
     * execution of the operation on the server side. However it will make
     * sure that the callback associated with this operation will not be
     * called anymore, effectively disabling the operation from the client
     * side's view. */
    void cancel();

    /** Return the current status of the operation */
    State getState() const;

    /** Set the callback function that is called when the operation state
     * changes. Usually this is not necessary, since the functions that
     * create pa_operation objects already take a callback that is called
     * when the operation finishes. Registering a state change callback is
     * mainly useful, if you want to get called back also if the operation
     * gets cancelled. \since 4.0 */
    void setStateCallback(notify_cb_t cb, void *userdata);

private:
    typedef void (*cb_wraper_t)(void* userdata);

    iIPCOperation(iIPCContext* c, iIPCStream* s, cb_wraper_t cb, void *userdata);

    void done();
    void unlink();
    void setState(State st);

    iAtomicCounter<int> m_ref;
    iIPCContext*    m_context;
    iIPCStream*     m_stream;

    State           m_state;
    void*           m_userdata;
    cb_wraper_t     m_callback;
    void*           m_state_userdata;
    notify_cb_t     m_state_callback;

    void*           m_private; /* some operations might need this */

    friend class iIPCStream;
    friend class iIPCContext;
    IX_DISABLE_COPY(iIPCOperation)
};

} // namespace iShell

#endif // IIPCOPERATION_H
