/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediatimerange.cpp
/// @brief   represents a time interval with integer precision
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "multimedia/imediatimerange.h"

namespace iShell {

/*!
    \class iMediaTimeInterval
    \brief The iMediaTimeInterval class represents a time interval with integer precision.

    \ingroup multimedia
    \ingroup multimedia_core

    An interval is specified by an inclusive start() and end() time.  These
    must be set in the constructor, as this is an immutable class.  The
    specific units of time represented by the class have not been defined - it
    is suitable for any times which can be represented by a signed 64 bit
    integer.

    The isNormal() method determines if a time interval is normal (a normal
    time interval has start() <= end()). A normal interval can be received
    from an abnormal interval by calling the normalized() method.

    The contains() method determines if a specified time lies within the time
    interval.

    The translated() method returns a time interval which has been translated
    forwards or backwards through time by a specified offset.

    \sa iMediaTimeRange
*/

/*!
    \fn iMediaTimeInterval::iMediaTimeInterval()

    Constructs an empty interval.
*/
iMediaTimeInterval::iMediaTimeInterval()
    : s(0)
    , e(0)
{
}

/*!
    \fn iMediaTimeInterval::iMediaTimeInterval(xint64 start, xint64 end)

    Constructs an interval with the specified \a start and \a end times.
*/
iMediaTimeInterval::iMediaTimeInterval(xint64 start, xint64 end)
    : s(start)
    , e(end)
{
}

/*!
    \fn iMediaTimeInterval::iMediaTimeInterval(const iMediaTimeInterval &other)

    Constructs an interval by taking a copy of \a other.
*/
iMediaTimeInterval::iMediaTimeInterval(const iMediaTimeInterval &other)
    : s(other.s)
    , e(other.e)
{
}

/*!
    \fn iMediaTimeInterval::start() const

    Returns the start time of the interval.

    \sa end()
*/
xint64 iMediaTimeInterval::start() const
{
    return s;
}

/*!
    \fn iMediaTimeInterval::end() const

    Returns the end time of the interval.

    \sa start()
*/
xint64 iMediaTimeInterval::end() const
{
    return e;
}

/*!
    \fn iMediaTimeInterval::contains(xint64 time) const

    Returns true if the time interval contains the specified \a time.
    That is, start() <= time <= end().
*/
bool iMediaTimeInterval::contains(xint64 time) const
{
    return isNormal() ? (s <= time && time <= e)
        : (e <= time && time <= s);
}

/*!
    \fn iMediaTimeInterval::isNormal() const

    Returns true if this time interval is normal.
    A normal time interval has start() <= end().

    \sa normalized()
*/
bool iMediaTimeInterval::isNormal() const
{
    return s <= e;
}

/*!
    \fn iMediaTimeInterval::normalized() const

    Returns a normalized version of this interval.

    If the start() time of the interval is greater than the end() time,
    then the returned interval has the start and end times swapped.
*/
iMediaTimeInterval iMediaTimeInterval::normalized() const
{
    if(s > e)
        return iMediaTimeInterval(e, s);

    return *this;
}

/*!
    \fn iMediaTimeInterval::translated(xint64 offset) const

    Returns a copy of this time interval, translated by a value of \a offset.
    An interval can be moved forward through time with a positive offset, or backward
    through time with a negative offset.
*/
iMediaTimeInterval iMediaTimeInterval::translated(xint64 offset) const
{
    return iMediaTimeInterval(s + offset, e + offset);
}

/*!
    \relates iMediaTimeRange

    Returns true if \a a is exactly equal to \a b.
*/
bool operator==(const iMediaTimeInterval &a, const iMediaTimeInterval &b)
{
    return a.start() == b.start() && a.end() == b.end();
}

/*!
    \relates iMediaTimeRange

    Returns true if \a a is not exactly equal to \a b.
*/
bool operator!=(const iMediaTimeInterval &a, const iMediaTimeInterval &b)
{
    return a.start() != b.start() || a.end() != b.end();
}

class iMediaTimeRangePrivate : public iSharedData
{
public:

    iMediaTimeRangePrivate();
    iMediaTimeRangePrivate(const iMediaTimeRangePrivate &other);
    iMediaTimeRangePrivate(const iMediaTimeInterval &interval);

    std::list<iMediaTimeInterval> intervals;

    void addInterval(const iMediaTimeInterval &interval);
    void removeInterval(const iMediaTimeInterval &interval);
};

iMediaTimeRangePrivate::iMediaTimeRangePrivate()
{}

iMediaTimeRangePrivate::iMediaTimeRangePrivate(const iMediaTimeRangePrivate &other)
    : intervals(other.intervals)
{}

iMediaTimeRangePrivate::iMediaTimeRangePrivate(const iMediaTimeInterval &interval)
{
    if(interval.isNormal())
        intervals.push_back(interval);
}

void iMediaTimeRangePrivate::addInterval(const iMediaTimeInterval &interval)
{
    // Handle normalized intervals only
    if(!interval.isNormal())
        return;

    // Find a place to insert the interval
    int idx = 0;
    std::list<iMediaTimeInterval>::iterator it = intervals.begin();
    for (it = intervals.begin(), idx = 0; it != intervals.end(); ++it, ++idx) {
        // Insert before this element
        if(interval.s < it->s) {
            intervals.insert(it, interval);
            break;
        }
    }

    // Interval needs to be added to the end of the list
    if (it == intervals.end())
        intervals.push_back(interval);

    // Do we need to correct the element before us?
    std::list<iMediaTimeInterval>::iterator tmpIt = intervals.begin();
    if (idx > 0)
        std::advance(tmpIt, idx - 1);
    if(idx > 0 && tmpIt->e >= interval.s - 1)
        idx--;

    // Merge trailing ranges
    while (idx < intervals.size() - 1) {
        it = intervals.begin();
        tmpIt = intervals.begin();
        std::advance(it, idx);
        std::advance(tmpIt, idx + 1);

        if (it->e < tmpIt->s - 1)
            break;

        it->e = std::max(it->e, tmpIt->e);
        intervals.erase(tmpIt);
    }
}

void iMediaTimeRangePrivate::removeInterval(const iMediaTimeInterval &interval)
{
    // Handle normalized intervals only
    if(!interval.isNormal())
        return;

    std::list<iMediaTimeInterval>::iterator it = intervals.begin();
    while (it != intervals.end()) {
        iMediaTimeInterval r = *it;

        if (r.e < interval.s) {
            // Before the removal interval
        } else if (interval.e < r.s) {
            // After the removal interval - stop here
            break;
        } else if (r.s < interval.s && interval.e < r.e) {
            // Split case - a single range has a chunk removed
            it->e = interval.s -1;
            addInterval(iMediaTimeInterval(interval.e + 1, r.e));
            break;
        } else if (r.s < interval.s) {
            // Trimming Tail Case
            it->e = interval.s - 1;
        } else if (interval.e < r.e) {
            // Trimming Head Case - we can stop after this
            it->s = interval.e + 1;
            break;
        } else {
            // Complete coverage case
            it = intervals.erase(it);
            continue;
        }

        ++it;
    }
}

/*!
    \class iMediaTimeRange
    \brief The iMediaTimeRange class represents a set of zero or more disjoint
    time intervals.
    \ingroup multimedia

    \reentrant

    The earliestTime(), latestTime(), intervals() and isEmpty()
    methods are used to get information about the current time range.

    The addInterval(), removeInterval() and clear() methods are used to modify
    the current time range.

    When adding or removing intervals from the time range, existing intervals
    within the range may be expanded, trimmed, deleted, merged or split to ensure
    that all intervals within the time range remain distinct and disjoint. As a
    consequence, all intervals added or removed from a time range must be
    \l{iMediaTimeInterval::isNormal()}{normal}.

    \sa iMediaTimeInterval
*/

/*!
    \fn iMediaTimeRange::iMediaTimeRange()

    Constructs an empty time range.
*/
iMediaTimeRange::iMediaTimeRange()
    : d(new iMediaTimeRangePrivate)
{
}

/*!
    \fn iMediaTimeRange::iMediaTimeRange(xint64 start, xint64 end)

    Constructs a time range that contains an initial interval from
    \a start to \a end inclusive.

    If the interval is not \l{iMediaTimeInterval::isNormal()}{normal},
    the resulting time range will be empty.

    \sa addInterval()
*/
iMediaTimeRange::iMediaTimeRange(xint64 start, xint64 end)
    : d(new iMediaTimeRangePrivate(iMediaTimeInterval(start, end)))
{
}

/*!
    \fn iMediaTimeRange::iMediaTimeRange(const iMediaTimeInterval &interval)

    Constructs a time range that contains an initial interval, \a interval.

    If \a interval is not \l{iMediaTimeInterval::isNormal()}{normal},
    the resulting time range will be empty.

    \sa addInterval()
*/
iMediaTimeRange::iMediaTimeRange(const iMediaTimeInterval &interval)
    : d(new iMediaTimeRangePrivate(interval))
{
}

/*!
    \fn iMediaTimeRange::iMediaTimeRange(const iMediaTimeRange &range)

    Constructs a time range by copying another time \a range.
*/
iMediaTimeRange::iMediaTimeRange(const iMediaTimeRange &range)
    : d(range.d)
{
}

/*!
    \fn iMediaTimeRange::~iMediaTimeRange()

    Destructor.
*/
iMediaTimeRange::~iMediaTimeRange()
{
}

/*!
    Takes a copy of the \a other time range and returns itself.
*/
iMediaTimeRange &iMediaTimeRange::operator=(const iMediaTimeRange &other)
{
    d = other.d;
    return *this;
}

/*!
    Sets the time range to a single continuous interval, \a interval.
*/
iMediaTimeRange &iMediaTimeRange::operator=(const iMediaTimeInterval &interval)
{
    d = new iMediaTimeRangePrivate(interval);
    return *this;
}

/*!
    \fn iMediaTimeRange::earliestTime() const

    Returns the earliest time within the time range.

    For empty time ranges, this value is equal to zero.

    \sa latestTime()
*/
xint64 iMediaTimeRange::earliestTime() const
{
    if (!d->intervals.empty())
        return d->intervals.front().s;

    return 0;
}

/*!
    \fn iMediaTimeRange::latestTime() const

    Returns the latest time within the time range.

    For empty time ranges, this value is equal to zero.

    \sa earliestTime()
*/
xint64 iMediaTimeRange::latestTime() const
{
    if (!d->intervals.empty())
        return d->intervals.back().e;

    return 0;
}

/*!
    \fn iMediaTimeRange::addInterval(xint64 start, xint64 end)
    \overload

    Adds the interval specified by \a start and \a end
    to the time range.

    \sa addInterval()
*/
void iMediaTimeRange::addInterval(xint64 start, xint64 end)
{
    d->addInterval(iMediaTimeInterval(start, end));
}

/*!
    \fn iMediaTimeRange::addInterval(const iMediaTimeInterval &interval)

    Adds the specified \a interval to the time range.

    Adding intervals which are not \l{iMediaTimeInterval::isNormal()}{normal}
    is invalid, and will be ignored.

    If the specified interval is adjacent to, or overlaps existing
    intervals within the time range, these intervals will be merged.

    This operation takes linear time.

    \sa removeInterval()
*/
void iMediaTimeRange::addInterval(const iMediaTimeInterval &interval)
{
    d->addInterval(interval);
}

/*!
    Adds each of the intervals in \a range to this time range.

    Equivalent to calling addInterval() for each interval in \a range.
*/
void iMediaTimeRange::addTimeRange(const iMediaTimeRange &range)
{
    const std::list<iMediaTimeInterval> intervals = range.intervals();
    for (std::list<iMediaTimeInterval>::const_iterator it = intervals.begin(); it != intervals.end(); ++it) {
        const iMediaTimeInterval &i = *it;
        d->addInterval(i);
    }
}

/*!
    \fn iMediaTimeRange::removeInterval(xint64 start, xint64 end)
    \overload

    Removes the interval specified by \a start and \a end
    from the time range.

    \sa removeInterval()
*/
void iMediaTimeRange::removeInterval(xint64 start, xint64 end)
{
    d->removeInterval(iMediaTimeInterval(start, end));
}

/*!
    \fn iMediaTimeRange::removeInterval(const iMediaTimeInterval &interval)

    Removes the specified \a interval from the time range.

    Removing intervals which are not \l{iMediaTimeInterval::isNormal()}{normal}
    is invalid, and will be ignored.

    Intervals within the time range will be trimmed, split or deleted
    such that no intervals within the time range include any part of the
    target interval.

    This operation takes linear time.

    \sa addInterval()
*/
void iMediaTimeRange::removeInterval(const iMediaTimeInterval &interval)
{
    d->removeInterval(interval);
}

/*!
    Removes each of the intervals in \a range from this time range.

    Equivalent to calling removeInterval() for each interval in \a range.
*/
void iMediaTimeRange::removeTimeRange(const iMediaTimeRange &range)
{
    const std::list<iMediaTimeInterval> intervals = range.intervals();
    for (std::list<iMediaTimeInterval>::const_iterator it = intervals.begin(); it != intervals.end(); ++it) {
        const iMediaTimeInterval &i = *it;
        d->removeInterval(i);
    }
}

/*!
    Adds each interval in \a other to the time range and returns the result.
*/
iMediaTimeRange& iMediaTimeRange::operator+=(const iMediaTimeRange &other)
{
    addTimeRange(other);
    return *this;
}

/*!
    Adds the specified \a interval to the time range and returns the result.
*/
iMediaTimeRange& iMediaTimeRange::operator+=(const iMediaTimeInterval &interval)
{
    addInterval(interval);
    return *this;
}

/*!
    Removes each interval in \a other from the time range and returns the result.
*/
iMediaTimeRange& iMediaTimeRange::operator-=(const iMediaTimeRange &other)
{
    removeTimeRange(other);
    return *this;
}

/*!
    Removes the specified \a interval from the time range and returns the result.
*/
iMediaTimeRange& iMediaTimeRange::operator-=(const iMediaTimeInterval &interval)
{
    removeInterval(interval);
    return *this;
}

/*!
    \fn iMediaTimeRange::clear()

    Removes all intervals from the time range.

    \sa removeInterval()
*/
void iMediaTimeRange::clear()
{
    d->intervals.clear();
}

/*!
    \fn iMediaTimeRange::intervals() const

    Returns the list of intervals covered by this time range.
*/
std::list<iMediaTimeInterval> iMediaTimeRange::intervals() const
{
    return d->intervals;
}

/*!
    \fn iMediaTimeRange::isEmpty() const

    Returns true if there are no intervals within the time range.

    \sa intervals()
*/
bool iMediaTimeRange::isEmpty() const
{
    return d->intervals.empty();
}

/*!
    \fn iMediaTimeRange::isContinuous() const

    Returns true if the time range consists of a continuous interval.
    That is, there is one or fewer disjoint intervals within the time range.
*/
bool iMediaTimeRange::isContinuous() const
{
    return (d->intervals.size() <= 1);
}

/*!
    \fn iMediaTimeRange::contains(xint64 time) const

    Returns true if the specified \a time lies within the time range.
*/
bool iMediaTimeRange::contains(xint64 time) const
{
    for (std::list<iMediaTimeInterval>::const_iterator it = d->intervals.begin(); it != d->intervals.end(); ++it) {
        if (it->contains(time))
            return true;

        if (time < it->s)
            break;
    }

    return false;
}

/*!
    \relates iMediaTimeRange

    Returns true if all intervals in \a a are present in \a b.
*/
bool operator==(const iMediaTimeRange &a, const iMediaTimeRange &b)
{
    return a.intervals() == b.intervals();
}

/*!
    \relates iMediaTimeRange

    Returns true if one or more intervals in \a a are not present in \a b.
*/
bool operator!=(const iMediaTimeRange &a, const iMediaTimeRange &b)
{
    return !(a == b);
}

/*!
    \relates iMediaTimeRange

    Returns a time range containing the union between \a r1 and \a r2.
 */
iMediaTimeRange operator+(const iMediaTimeRange &r1, const iMediaTimeRange &r2)
{
    return (iMediaTimeRange(r1) += r2);
}

/*!
    \relates iMediaTimeRange

    Returns a time range containing \a r2 subtracted from \a r1.
 */
iMediaTimeRange operator-(const iMediaTimeRange &r1, const iMediaTimeRange &r2)
{
    return (iMediaTimeRange(r1) -= r2);
}

} // namespace iShell
