#ifndef IVIDEOACCESSUNIT_H
#define IVIDEOACCESSUNIT_H

#include <core/utils/ibytearray.h>
#include <multimedia/imultimediaglobal.h>

namespace iShell {

/// One Annex-B access unit. Times are microseconds relative to the session
/// origin, matching iVideoFrame. DTS may be negative for decoder preroll.
class IX_MULTIMEDIA_EXPORT iVideoAccessUnit
{
public:
    static const xint64 InvalidTime;
    iVideoAccessUnit();

    iByteArray data;
    xuint64 frameId;
    xint64 pts;
    xint64 dts;
    xint64 duration;
    bool keyFrame;
};

} // namespace iShell

#endif // IVIDEOACCESSUNIT_H