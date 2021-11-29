/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediaobject.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIAOBJECT_H
#define IMEDIAOBJECT_H

#include <unordered_set>
#include <core/kernel/iobject.h>
#include <core/kernel/itimer.h>

#include <multimedia/imultimediaglobal.h>
#include <multimedia/imultimedia.h>

namespace iShell {

class IX_MULTIMEDIA_EXPORT iMediaObject : public iObject
{
    IX_OBJECT(iMediaObject)
public:
    ~iMediaObject();

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
    void notifyIntervalChanged(int milliSeconds) ISIGNAL(notifyIntervalChanged, milliSeconds)

    void metaDataAvailableChanged(bool available) ISIGNAL(metaDataAvailableChanged, available)
    void metaDataChanged(const iString &key, const iVariant &value) ISIGNAL(metaDataChanged, key, value)

    void availabilityChanged(iMultimedia::AvailabilityStatus availability) ISIGNAL(availabilityChanged, availability)

protected:
    iMediaObject(iObject *parent);

    void addPropertyWatch(iByteArray const &name);
    void removePropertyWatch(iByteArray const &name);

private:
    void setupControls();
    void timeoutNotify();

    iTimer m_notifyTimer;
    std::unordered_set<iString, iKeyHashFunc> m_notifyProperties;
};

} // namespace iShell

#endif // IMEDIAOBJECT_H
