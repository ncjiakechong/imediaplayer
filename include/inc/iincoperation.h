/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincoperation.h
/// @brief   async operation of INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCOPERATION_H
#define IINCOPERATION_H

#include <core/global/imacro.h>
#include <core/thread/iatomiccounter.h>
#include <inc/iincglobal.h>

namespace iShell {

class iINCStream;
class iINCContext;

class IX_INC_EXPORT iINCOperation
{
public:
    /** A callback for operation state changes */
    typedef void (*notify_cb_t) (iINCOperation* o, void* userdata);

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

    iINCOperation(iINCContext* c, iINCStream* s, cb_wraper_t cb, void *userdata);
    ~iINCOperation();

    void done();
    void unlink();
    void setState(State st);

    iAtomicCounter<int> m_ref;
    iINCContext*    m_context;
    iINCStream*     m_stream;

    State           m_state;
    void*           m_userdata;
    cb_wraper_t     m_callback;
    void*           m_state_userdata;
    notify_cb_t     m_state_callback;

    void*           m_private; /* some operations might need this */

    friend class iINCStream;
    friend class iINCContext;
    IX_DISABLE_COPY(iINCOperation)
};

} // namespace iShell

#endif // IINCOPERATION_H
