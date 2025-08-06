/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventloop.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IEVENTLOOP_H
#define IEVENTLOOP_H

#include <core/kernel/iobject.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {

class iEventLoop : public iObject
{
    IX_OBJECT(iEventLoop)
public:
    iEventLoop(iObject* parent = IX_NULLPTR);
    virtual ~iEventLoop();

    int exec();
    void exit(int returnCode = 0);

protected:
    virtual bool event(iEvent *e);

    bool processEvents();

    iAtomicCounter<int> m_exit; // bool
    iAtomicCounter<int> m_returnCode;
};

} // namespace iShell

#endif // IEVENTLOOP_H
