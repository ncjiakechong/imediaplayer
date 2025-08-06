/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icoreapplication.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ICOREAPPLICATION_H
#define ICOREAPPLICATION_H

#include <core/kernel/iobject.h>
#include <core/thread/iatomiccounter.h>

namespace iShell {

class iPostEvent;
class iEventDispatcher;

struct IX_CORE_EXPORT iCoreApplicationPrivate
{
    virtual ~iCoreApplicationPrivate();
    virtual iEventDispatcher* createEventDispatcher() const;
};

class IX_CORE_EXPORT iCoreApplication : public iObject
{
    IX_OBJECT(iCoreApplication)
public:
    iCoreApplication(int argc, char **argv);
    virtual ~iCoreApplication();

    static iCoreApplication *instance() { return self; }

    static int exec();
    static void exit(int retCode = 0);

    static bool sendEvent(iObject *receiver, iEvent *event);
    static void postEvent(iObject *receiver, iEvent *event);
    static void removePostedEvents(iObject *receiver, int eventType);

    static void sendPostedEvents(iObject *receiver, int event_type = 0);

    static iEventDispatcher* createEventDispatcher();

    iEventDispatcher* eventDispatcher() const;

protected:
    iCoreApplication(iCoreApplicationPrivate* priv);

    virtual bool event(iEvent *);
    virtual bool notify(iObject *, iEvent *);

    virtual bool compressEvent(iEvent *, iObject *receiver, std::list<iPostEvent> *);

    static bool threadRequiresCoreApplication();
    static bool doNotify(iObject *receiver, iEvent *event);

private:
    void init();

    static iCoreApplication* self;

    iCoreApplicationPrivate* m_private;
    friend class iEventLoop;
};

} // namespace iShell

#endif // ICOREAPPLICATION_H
