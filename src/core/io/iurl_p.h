/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurl_p.h
/// @brief   provides a convenient interface for working with URLs
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
int ix_urlRecode(iString &appendTo, iStringView url, iUrl::ComponentFormattingOptions encoding, const xuint16 *tableModifications = IX_NULLPTR);
xsizetype ix_encodeFromUser(iString &appendTo, const iString &input, const xuint16 *tableModifications);

// in iurlidna.cpp
enum AceLeadingDot { AllowLeadingDot, ForbidLeadingDot };
enum AceOperation { ToAceOnly, NormalizeAce };
iString ix_ACE_do(const iString &domain, AceOperation op, AceLeadingDot dot, iUrl::AceProcessingOptions options = 0);
void ix_punycodeEncoder(iStringView in, iString *output);
iString ix_punycodeDecoder(const iString &pc);

} // namespace iShell

#endif // IURL_P_H
