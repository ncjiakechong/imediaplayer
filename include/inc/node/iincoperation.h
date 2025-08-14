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

#include <inc/kernel/iincglobal.h>
#include <core/kernel/iobject.h>

namespace iShell {

class iINCStream;
class iINCContext;

class IX_INC_EXPORT iINCOperation : public iObject
{
    IX_OBJECT(iINCOperation)
public:
    /** The state of an operation */
    enum State {
        STATE_RUNNING,      /**< The operation is still running */
        STATE_DONE,         /**< The operation has completed */
        STATE_FAILED,       /**< The operation has failed */
        STATE_TIMEOUT,      /**< The operation has timed out */
        STATE_CANCELLED
        /**< The operation has been cancelled. Operations may get cancelled by the
         * application, or as a result of the context getting disconnected while the
         * operation is pending. */
    };

    /** Cancel the operation. Beware! This will not necessarily cancel the
     * execution of the operation on the server side. However it will make
     * sure that the callback associated with this operation will not be
     * called anymore, effectively disabling the operation from the client
     * side's view. */
    inline void cancel() { setState(STATE_CANCELLED); }

    /** Return the current status of the operation */
    inline State getState() const { return m_state; }

public: // signal
    void stateChanged(State now, State pre) ISIGNAL(stateChanged, now, pre);

private:
    iINCOperation(iINCContext* c, iINCStream* s);
    virtual ~iINCOperation();

    void unlink();
    void setState(State st);
    inline void done() { setState(STATE_DONE); }

    iINCContext*    m_context;
    iINCStream*     m_stream;

    State           m_state;

    friend class iINCStream;
    friend class iINCContext;
    IX_DISABLE_COPY(iINCOperation)
};

} // namespace iShell

#endif // IINCOPERATION_H
