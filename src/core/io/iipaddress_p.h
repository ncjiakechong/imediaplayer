/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iipaddress_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IIPADDRESS_P_H
#define IIPADDRESS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the public API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <core/global/iglobal.h>
#include <core/utils/istring.h>

namespace iShell {

namespace iIPAddressUtils {

typedef xuint32 IPv4Address;
typedef xuint8 IPv6Address[16];

IX_CORE_EXPORT bool parseIp4(IPv4Address &address, const iChar *begin, const iChar *end);
IX_CORE_EXPORT const iChar *parseIp6(IPv6Address &address, const iChar *begin, const iChar *end);
IX_CORE_EXPORT void toString(iString &appendTo, IPv4Address address);
IX_CORE_EXPORT void toString(iString &appendTo, IPv6Address address);

} // namespace

} // namespace iShell

#endif // IIPADDRESS_P_H
