/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventloop.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IEVENTLOOP_H
#define IEVENTLOOP_H

#include <core/kernel/iobject.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {

class IX_CORE_EXPORT iEventLoop : public iObject
{
    IX_OBJECT(iEventLoop)
public:
    enum ProcessEventsFlag {
        AllEvents = 0x00,
        WaitForMoreEvents = 0x01,
        EventLoopExec = 0x02
    };
    typedef uint ProcessEventsFlags;

    iEventLoop(iObject* parent = IX_NULLPTR);
    virtual ~iEventLoop();

    bool processEvents(ProcessEventsFlags flags = AllEvents);

    int exec(ProcessEventsFlags flags = AllEvents);
    void exit(int returnCode = 0);

protected:
    virtual bool event(iEvent *e);

    bool m_inExec;
    iAtomicCounter<int> m_exit; // bool
    iAtomicCounter<int> m_returnCode;
};

} // namespace iShell

#endif // IEVENTLOOP_H
