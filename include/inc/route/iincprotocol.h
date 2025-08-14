/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincprotocal.h
/// @brief   protocal of router
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCPROTOCOL_H
#define IINCPROTOCOL_H

#include <inc/kernel/iincengine.h>

namespace iShell {

class IX_INC_EXPORT iINCProtocal : public iObject
{
    IX_OBJECT(iINCProtocal)
public:
    iINCProtocal(const iStringView& name, iINCEngine *engine, iObject *parent = IX_NULLPTR);

protected:
    virtual ~iINCProtocal();

private:
    IX_DISABLE_COPY(iINCProtocal)
};

} // namespace iShell

#endif // IINCPROTOCOL_H