/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ievent.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-11-6
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-11-6          anfengce@        Create.
/////////////////////////////////////////////////////////////////
#ifndef IEVENT_H
#define IEVENT_H

namespace ishell {

class iObject;

class iEvent
{
public:
    enum Type {
        None = 0,                               // invalid event
        Timer,
        MetaCall,
        ThreadChange,
        ChildAdded,                             // new child widget
        ChildRemoved,
        Quit,

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
    unsigned short m_reserved : 13;
};

class iTimerEvent : public iEvent
{
public:
    explicit iTimerEvent( int timerId );
    ~iTimerEvent();
    int timerId() const { return id; }
protected:
    int id;
};

class iChildEvent : public iEvent
{
public:
    iChildEvent(Type type, iObject *child);
    ~iChildEvent();
    iObject *child() const { return c; }
    bool added() const { return type() == ChildAdded; }
    bool removed() const { return type() == ChildRemoved; }

protected:
    iObject *c;
};

} // namespace ishell

#endif // IEVENT_H
