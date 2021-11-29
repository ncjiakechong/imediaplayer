/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventsource.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IEVENTSOURCE_H
#define IEVENTSOURCE_H

#include <list>
#include <core/kernel/ipoll.h>

namespace iShell {

class iEventDispatcher;

typedef enum
{
  IX_EVENT_SOURCE_READY = 1 << 0,
  IX_EVENT_SOURCE_CAN_RECURSE = 1 << 1,
  IX_EVENT_SOURCE_BLOCKED = 1 << 2
} iEventSourceFlags;

class IX_CORE_EXPORT iEventSource
{
public:
    iEventSource(int priority);

    void ref();
    void deref();

    int attach(iEventDispatcher* dispatcher);
    int detach();

    int addPoll(iPollFD* fd);
    int removePoll(iPollFD* fd);

    int priority() const { return m_priority; }

    int flags() const { return m_flags; }
    void setFlags(int flags) { m_flags = flags; }


    /**
     * Called before all the file descriptors are polled. If the
     * source can determine that it is ready here (without waiting for the
     * results of the poll() call) it should return %TRUE. It can also return
     * a @timeout_ value which should be the maximum timeout (in milliseconds)
     * which should be passed to the poll() call. The actual timeout used will
     * be -1 if all sources returned -1, or it will be the minimum of all
     * the @timeout_ values returned which were >= 0.  Since this may
     * be %IX_NULLPTR, in which case the effect is as if the function always returns
     * %FALSE with a timeout of -1.
     */
    virtual bool prepare(int *timeout_);

    /**
     * Called after all the file descriptors are polled. The source
     * should return %TRUE if it is ready to be dispatched. Note that some
     * time may have passed since the previous prepare function was called,
     * so the source should be checked again here.
     */
    virtual bool check();

    /**
     * Called to dispatch the event source, after it has returned
     * %TRUE in either its @prepare or its @check function.
     * The @dispatch function receives a callback function and
     * user data. The return value of the @dispatch function
     * should be removed(%FALSE) or continue to keep it(%TRUE).
     */
    virtual bool dispatch();

protected:
    virtual ~iEventSource();

private:
    int m_priority;
    int m_refCount;

    int m_flags;

    iEventDispatcher* m_dispatcher;
    std::list<iPollFD*> m_pollFds;
};

} // namespace iShell

#endif // IEVENTSOURCE_H
