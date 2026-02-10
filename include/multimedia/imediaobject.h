/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaobject.h
/// @brief   provides a common base for multimedia objects
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIAOBJECT_H
#define IMEDIAOBJECT_H

#include <set>
#if __cplusplus >= 201103L
#include <unordered_set>
#endif

#include <core/kernel/iobject.h>
#include <core/kernel/itimer.h>

#include <multimedia/imultimediaglobal.h>
#include <multimedia/imultimedia.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iMediaObject : public iObject
{
    IX_OBJECT(iMediaObject)
public:
    virtual ~iMediaObject();

    virtual bool isAvailable() const;
    virtual iMultimedia::AvailabilityStatus availability() const;

    int notifyInterval() const;
    void setNotifyInterval(int milliSeconds);

    virtual bool bind(iObject *);
    virtual void unbind(iObject *);

    bool isMetaDataAvailable() const;

    iVariant metaData(const iString &key) const;
    std::list<iString> availableMetaData() const;

public: // signal
    void notifyIntervalChanged(int milliSeconds) ISIGNAL(notifyIntervalChanged, milliSeconds);

    void metaDataAvailableChanged(bool available) ISIGNAL(metaDataAvailableChanged, available);
    void metaDataChanged(iString key, iVariant value) ISIGNAL(metaDataChanged, key, value);

    void availabilityChanged(iMultimedia::AvailabilityStatus availability) ISIGNAL(availabilityChanged, availability);

protected:
    iMediaObject(iObject *parent);

    void addPropertyWatch(iByteArray const &name);
    void removePropertyWatch(iByteArray const &name);

private:
    void setupControls();
    void timeoutNotify();

    iTimer m_notifyTimer;

    #if __cplusplus >= 201103L
    typedef std::unordered_set<iString, iKeyHashFunc> PropertySet;
    #else
    typedef std::set<iString> PropertySet;
    #endif
    PropertySet m_notifyProperties;
};

} // namespace iShell

#endif // IMEDIAOBJECT_H
