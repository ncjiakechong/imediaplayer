/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipcstream.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IIPCSTREAM_H
#define IIPCSTREAM_H

#include <core/kernel/iobject.h>
#include <ipc/iipcglobal.h>

namespace iShell {

class iIPCContext;
class iIPCOperation;

class IX_IPC_EXPORT iIPCStream : public iObject
{
    IX_OBJECT(iIPCStream)
public:
    iIPCStream(iIPCContext* context);

public: // signal

protected:
    virtual ~iIPCStream();

private:

    friend class iIPCOperation;
};

} // namespace iShell

#endif // IIPCSTREAM_H
