#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T09:31:30
#
#-------------------------------------------------

SOURCES += \
    $$PWD/ieventdispatcher_generic.cpp \
    thread/icondition.cpp \
    thread/isemaphore.cpp \
    thread/ithread.cpp \
    thread/imutex.cpp \
    thread/iwakeup.cpp

HEADERS += \
    $$PWD/ieventdispatcher_generic.h \
    $$PWD/iorderedmutexlocker_p.h \
    $$PWD/ithread_p.h \
    ../../include/core/thread/iatomiccounter.h \
    ../../include/core/thread/icondition.h \
    ../../include/core/thread/iscopedlock.h \
    ../../include/core/thread/ithread.h \
    ../../include/core/thread/iatomicpointer.h \
    ../../include/core/thread/imutex.h \
    ../../include/core/thread/isemaphore.h \
    ../../include/core/thread/iwakeup.h

unix {
    HEADERS += \
        $$PWD/ieventdispatcher_glib.h

    SOURCES += \
        $$PWD/ieventdispatcher_glib.cpp \
        $$PWD/ithread_posix.cpp

    QMAKE_USE += glib
} else {
    SOURCES += \
        thread/ithread_win.cpp

    LIBS += -lUser32
}
