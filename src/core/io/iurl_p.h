/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurl_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IURL_P_H
#define IURL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the public API. It exists for the convenience of
// iurl*.cpp This header file may change from version to version without
// notice, or even be removed.
//
// We mean it.
//

#include <core/io/iurl.h>

namespace iShell {

// in iurlrecode.cpp
extern int ix_urlRecode(iString &appendTo, const iChar *begin, const iChar *end,
                                          iUrl::ComponentFormattingOptions encoding, const ushort *tableModifications = 0);

// in iurlidna.cpp
enum AceLeadingDot { AllowLeadingDot, ForbidLeadingDot };
enum AceOperation { ToAceOnly, NormalizeAce };
extern iString ix_ACE_do(const iString &domain, AceOperation op, AceLeadingDot dot);
extern bool ix_nameprep(iString *source, int from);
extern bool ix_check_std3rules(const iChar *uc, int len);
extern void ix_punycodeEncoder(const iChar *s, int ucLength, iString *output);
extern iString ix_punycodeDecoder(const iString &pc);
extern iString ix_urlRecodeByteArray(const iByteArray &ba);

} // namespace iShell

#endif // IURL_P_H
