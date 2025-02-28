/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediatimerange.h
/// @brief   represents a time interval with integer precision
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIATIMERANGE_H
#define IMEDIATIMERANGE_H

#include <list>

#include <core/utils/ishareddata.h>
#include <multimedia/imultimediaglobal.h>

namespace iShell {


class iMediaTimeRangePrivate;

class IX_MULTIMEDIA_EXPORT iMediaTimeInterval
{
public:
    iMediaTimeInterval();
    iMediaTimeInterval(xint64 start, xint64 end);
    iMediaTimeInterval(const iMediaTimeInterval&);

    xint64 start() const;
    xint64 end() const;

    bool contains(xint64 time) const;

    bool isNormal() const;
    iMediaTimeInterval normalized() const;
    iMediaTimeInterval translated(xint64 offset) const;

private:
    friend class iMediaTimeRangePrivate;
    friend class iMediaTimeRange;

    xint64 s;
    xint64 e;
};

IX_MULTIMEDIA_EXPORT bool operator==(const iMediaTimeInterval&, const iMediaTimeInterval&);
IX_MULTIMEDIA_EXPORT bool operator!=(const iMediaTimeInterval&, const iMediaTimeInterval&);

class IX_MULTIMEDIA_EXPORT iMediaTimeRange
{
public:

    iMediaTimeRange();
    iMediaTimeRange(xint64 start, xint64 end);
    iMediaTimeRange(const iMediaTimeInterval&);
    iMediaTimeRange(const iMediaTimeRange &range);
    ~iMediaTimeRange();

    iMediaTimeRange &operator=(const iMediaTimeRange&);
    iMediaTimeRange &operator=(const iMediaTimeInterval&);

    xint64 earliestTime() const;
    xint64 latestTime() const;

    std::list<iMediaTimeInterval> intervals() const;
    bool isEmpty() const;
    bool isContinuous() const;

    bool contains(xint64 time) const;

    void addInterval(xint64 start, xint64 end);
    void addInterval(const iMediaTimeInterval &interval);
    void addTimeRange(const iMediaTimeRange&);

    void removeInterval(xint64 start, xint64 end);
    void removeInterval(const iMediaTimeInterval &interval);
    void removeTimeRange(const iMediaTimeRange&);

    iMediaTimeRange& operator+=(const iMediaTimeRange&);
    iMediaTimeRange& operator+=(const iMediaTimeInterval&);
    iMediaTimeRange& operator-=(const iMediaTimeRange&);
    iMediaTimeRange& operator-=(const iMediaTimeInterval&);

    void clear();

private:
    iExplicitlySharedDataPointer<iMediaTimeRangePrivate> d;
};

IX_MULTIMEDIA_EXPORT bool operator==(const iMediaTimeRange&, const iMediaTimeRange&);
IX_MULTIMEDIA_EXPORT bool operator!=(const iMediaTimeRange&, const iMediaTimeRange&);
IX_MULTIMEDIA_EXPORT iMediaTimeRange operator+(const iMediaTimeRange&, const iMediaTimeRange&);
IX_MULTIMEDIA_EXPORT iMediaTimeRange operator-(const iMediaTimeRange&, const iMediaTimeRange&);


} // namespace iShell

#endif // IMEDIATIMERANGE_H
