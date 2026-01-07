/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ievent.h
/// @brief   defines event subsystem in iObject and several derived classes for specific event types
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IEVENT_H
#define IEVENT_H

#include <core/global/iglobal.h>

namespace iShell {

class IX_CORE_EXPORT iEvent
{
public:
    enum Type {
        None = 0,                               // invalid event
        Timer,
        Quit,
        MetaCall,
        ThreadChange,
        ChildAdded,                             // new child widget
        ChildRemoved,
        DeferredDelete,

        /// user
        User = 1000,                            // first user event id
        MaxUser = 65535                         // last user event id
    };

    explicit iEvent(unsigned short type);
    iEvent(const iEvent &other);
    virtual ~iEvent();
    iEvent &operator=(const iEvent &other);
    inline unsigned short type() const { return m_type; }

    inline void setAccepted(bool accepted) { m_accept = accepted; }
    inline bool isAccepted() const { return m_accept; }

    inline void accept() { m_accept = true; }
    inline void ignore() { m_accept = false; }

    static int registerEventType(int hint = -1);
protected:
    unsigned short m_type;

    unsigned short m_posted : 1;
    unsigned short m_accept : 1;
    unsigned short m_reserved : 14;

    friend class iCoreApplication;
};

class IX_CORE_EXPORT iTimerEvent : public iEvent
{
public:
    explicit iTimerEvent(int timerId, xintptr userdata);
    virtual ~iTimerEvent();
    int timerId() const { return id; }
    xintptr userData() const { return userdata; }
protected:
    int id;
    xintptr userdata;
};

class iObject;
class IX_CORE_EXPORT iChildEvent : public iEvent
{
public:
    iChildEvent(Type type, iObject *child);
    virtual ~iChildEvent();
    iObject *child() const { return c; }
    bool added() const { return type() == ChildAdded; }
    bool removed() const { return type() == ChildRemoved; }

protected:
    iObject *c;
};

class IX_CORE_EXPORT iDeferredDeleteEvent : public iEvent
{
public:
    explicit iDeferredDeleteEvent();
    virtual ~iDeferredDeleteEvent();
    int loopLevel() const { return ll; }
    int scopeLevel() const { return sl; }
private:
    int ll;
    int sl;
    friend class iCoreApplication;
};

} // namespace iShell

#endif // IEVENT_H
