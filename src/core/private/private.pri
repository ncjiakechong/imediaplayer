#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T09:31:30
#
#-------------------------------------------------

SOURCES += \
    $$PWD/ibytearraymatcher.cpp \
    $$PWD/ilocale_tools_p.cpp \
    $$PWD/istringmatcher.cpp \
    $$PWD/iunicodetables_data.cpp \
    $$PWD/iutfcodec.cpp \
    private/ieventdispatcher_generic.cpp \
    private/itimerinfo.cpp

HEADERS += \
    $$PWD/ibytearraymatcher.h \
    $$PWD/iendian_p.h \
    $$PWD/ilocale_data_p.h \
    $$PWD/ilocale_p.h \
    $$PWD/ilocale_tools_p.h \
    $$PWD/inumeric_p.h \
    $$PWD/istringalgorithms_p.h \
    $$PWD/istringiterator_p.h \
    $$PWD/istringmatcher.h \
    $$PWD/itextcodec_p.h \
    $$PWD/itools_p.h \
    $$PWD/iunicodetables_data.h \
    $$PWD/iunicodetables_p.h \
    $$PWD/iutfcodec_p.h \
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
