/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurl_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
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
int ix_urlRecode(iString &appendTo, const iChar *begin, const iChar *end,
                                          iUrl::ComponentFormattingOptions encoding, const xuint16 *tableModifications = 0);

// in iurlidna.cpp
enum AceLeadingDot { AllowLeadingDot, ForbidLeadingDot };
enum AceOperation { ToAceOnly, NormalizeAce };
iString ix_ACE_do(const iString &domain, AceOperation op, AceLeadingDot dot);
bool ix_nameprep(iString *source, int from);
bool ix_check_std3rules(const iChar *uc, int len);
void ix_punycodeEncoder(const iChar *s, int ucLength, iString *output);
iString ix_punycodeDecoder(const iString &pc);
iString ix_urlRecodeByteArray(const iByteArray &ba);

} // namespace iShell

#endif // IURL_P_H
