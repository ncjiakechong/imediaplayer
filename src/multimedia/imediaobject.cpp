/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaobject.cpp
/// @brief   provides a common base for multimedia objects
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <multimedia/imediaobject.h>

namespace iShell {

/*!
    \class iMediaObject

    \brief The iMediaObject class provides a common base for multimedia objects.

    \ingroup multimedia
    \ingroup multimedia_core
*/

/*!
    Destroys this media object.
*/

iMediaObject::~iMediaObject()
{
}

/*!
    Returns the availability of the functionality offered by this object.

    In some cases the functionality may not be available (for example, if
    the current operating system or platform does not provide the required
    functionality), or it may be temporarily unavailable (for example,
    audio playback during a phone call or similar).
*/

iMultimedia::AvailabilityStatus iMediaObject::availability() const
{
    return iMultimedia::Available;
}

/*!
    Returns true if the service is available for use.
*/

bool iMediaObject::isAvailable() const
{
    return availability() == iMultimedia::Available;
}

/*!
    Returns the media service that provides the functionality of this multimedia object.
*/

int iMediaObject::notifyInterval() const
{
    return m_notifyTimer.interval();
}

void iMediaObject::setNotifyInterval(int milliSeconds)
{
    if (m_notifyTimer.interval() != milliSeconds) {
        m_notifyTimer.setInterval(milliSeconds);

        IEMIT notifyIntervalChanged(milliSeconds);
    }
}

/*!
    Bind \a object to this iMediaObject instance.

    This method establishes a relationship between this media object and a
    helper object. The nature of the relationship depends on both parties. This
    methods returns true if the helper was successfully bound, false otherwise.

    Most subclasses of iMediaObject provide more convenient functions
    that wrap this functionality, so this function rarely needs to be
    called directly.

    The object passed must implement the iMediaBindableInterface interface.

    \sa iMediaBindableInterface
*/
bool iMediaObject::bind(iObject *object)
{
    IX_UNUSED(object);
    return false;
}

/*!
    Detach \a object from the iMediaObject instance.

    Unbind the helper object from this media object.  A warning
    will be generated if the object was not previously bound to this
    object.

    \sa iMediaBindableInterface
*/
void iMediaObject::unbind(iObject *object)
{
    IX_UNUSED(object);
}

/*!
    Constructs a media object which uses the functionality provided by a media \a service.

    The \a parent is passed to iObject.

    This class is meant as a base class for multimedia objects so this
    constructor is protected.
*/

iMediaObject::iMediaObject(iObject *parent)
    : iObject(parent)
    , m_notifyTimer(IX_NULLPTR)
{
    m_notifyTimer.setInterval(1000);
    connect(&m_notifyTimer, &iTimer::timeout, this, &iMediaObject::timeoutNotify);

    setupControls();
}

void iMediaObject::timeoutNotify()
{
    for (PropertySet::const_iterator it = m_notifyProperties.begin(); it != m_notifyProperties.end(); ++it) {
        const iMetaObject* mo = metaObject();

        do {
            const _iProperty* tProperty = mo->property(iLatin1StringView(it->toUtf8().data()));
            if (IX_NULLPTR == tProperty)
                continue;

            iVariant value = tProperty->_get(tProperty, this);
            IEMIT tProperty->_signal(tProperty, this, value);
            break;
        } while ((mo = mo->superClass()));
    }
}

/*!
    Watch the property \a name. The property's notify signal will be emitted
    once every \c notifyInterval milliseconds.

    \sa notifyInterval
*/
void iMediaObject::addPropertyWatch(iByteArray const &name)
{
    const iMetaObject* mo = metaObject();

    do {
        const _iProperty* tProperty = mo->property(iLatin1StringView(name.data()));
        if (IX_NULLPTR == tProperty)
            continue;

        m_notifyProperties.insert(name);

        if (!m_notifyTimer.isActive())
            m_notifyTimer.start();

        return;
    } while ((mo = mo->superClass()));
}

/*!
    Remove property \a name from the list of properties whose changes are
    regularly signaled.

    \sa notifyInterval
*/

void iMediaObject::removePropertyWatch(iByteArray const &name)
{
    PropertySet::const_iterator it = m_notifyProperties.find(name);
    if (it != m_notifyProperties.end())
        m_notifyProperties.erase(it);

    if (m_notifyProperties.empty())
        m_notifyTimer.stop();
}

/*!
    \property iMediaObject::notifyInterval

    The interval at which notifiable properties will update.

    The interval is expressed in milliseconds, the default value is 1000.

    \sa addPropertyWatch(), removePropertyWatch()
*/

/*!
    \fn void iMediaObject::notifyIntervalChanged(int milliseconds)

    Signal a change in the notify interval period to \a milliseconds.
*/

/*!
    Returns true if there is meta-data associated with this media object, else false.
*/

bool iMediaObject::isMetaDataAvailable() const
{
    return false;
}

/*!
    \fn iMediaObject::metaDataAvailableChanged(bool available)

    Signals that the \a available state of a media object's meta-data has changed.
*/

/*!
    Returns the value associated with a meta-data \a key.

    See the list of predefined \l {iMediaMetaData}{meta-data keys}.
*/
iVariant iMediaObject::metaData(const iString &key) const
{
    IX_UNUSED(key);
    return iVariant();
}

/*!
    Returns a list of keys there is meta-data available for.
*/
std::list<iString> iMediaObject::availableMetaData() const
{
    return std::list<iString>();
}

/*!
    \fn iMediaObject::metaDataChanged()

    Signals that this media object's meta-data has changed.

    If multiple meta-data elements are changed,
    metaDataChanged(const iString &key, const iVariant &value) signal is emitted
    for each of them with metaDataChanged() changed emitted once.
*/

/*!
    \fn iMediaObject::metaDataChanged(const iString &key, const iVariant &value)

    Signal the changes of one meta-data element \a value with the given \a key.
*/


void iMediaObject::setupControls()
{
}

/*!
    \fn iMediaObject::availabilityChanged(bool available)

    Signal emitted when the availability state has changed to \a available.
*/

/*!
    \fn iMediaObject::availabilityChanged(iMultimedia::AvailabilityStatus availability)

    Signal emitted when the availability of the service has changed to \a availability.
*/


} // namespace iShell
