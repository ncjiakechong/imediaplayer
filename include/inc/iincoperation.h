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

#include <inc/iincglobal.h>
#include <core/global/imacro.h>
#include <core/utils/irefcount.h>

namespace iShell {

class iINCStream;
class iINCContext;

class IX_INC_EXPORT iINCOperation
{
public:
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
    bool ref() { return m_ref.ref(); }

    /** Decrease the reference count by one */
    bool deref();

    /** Cancel the operation. Beware! This will not necessarily cancel the
     * execution of the operation on the server side. However it will make
     * sure that the callback associated with this operation will not be
     * called anymore, effectively disabling the operation from the client
     * side's view. */
    inline void cancel() { setState(STATE_CANCELLED); }

    /** Return the current status of the operation */
    inline State getState() const { return m_state; }

protected:
    iINCOperation(iINCContext* c, iINCStream* s);

    /** A callback for operation state changes */
    virtual void stateChanges(State now, State pre) = 0;

    inline void done() { setState(STATE_DONE); }

private:
    virtual ~iINCOperation();

    void unlink();
    void setState(State st);

    iRefCount       m_ref;
    iINCContext*    m_context;
    iINCStream*     m_stream;

    State           m_state;

    friend class iINCStream;
    friend class iINCContext;
    IX_DISABLE_COPY(iINCOperation)
};

} // namespace iShell

#endif // IINCOPERATION_H
