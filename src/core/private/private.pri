#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T09:31:30
#
#-------------------------------------------------

SOURCES += \
    private/ieventdispatcher_generic.cpp \
    private/itimerinfo.cpp

HEADERS += \
    private/ithread_p.h \
    private/ieventdispatcher_generic.h \
    private/itimerinfo.h

unix {
    HEADERS += \
        private/icoreposix.h \
        private/ieventdispatcher_glib.h

    SOURCES += \
        private/ieventdispatcher_glib.cpp \
        private/icoreposix.cpp

    QMAKE_USE += glib
}
