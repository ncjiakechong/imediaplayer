/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ieventloop.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-11-6
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-11-6          anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef IEVENTLOOP_H
#define IEVENTLOOP_H

#include <core/kernel/iobject.h>
#include <core/thread/iatomiccounter.h>

namespace ishell {

class iEventLoop : public iObject
{
public:
    iEventLoop(iObject* parent = I_NULLPTR);
    virtual ~iEventLoop();

    int exec();
    void exit(int returnCode = 0);

protected:
    virtual bool event(iEvent *e);

    bool processEvents();

    iAtomicCounter<int> m_exit; // bool
    iAtomicCounter<int> m_returnCode;
};

} // namespace ishell

#endif // IEVENTLOOP_H
