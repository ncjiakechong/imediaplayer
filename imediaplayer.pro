TEMPLATE = subdirs

src_icorelib.file = $$PWD/src/core/icorelib.pro
src_icorelib.target = sub-icorelib

SUBDIRS += src_icorelib

src_multimedia.file = $$PWD/src/multimedia/multimedia.pro
src_multimedia.target = sub-multimedia
src_multimedia.depends = src_icorelib

SUBDIRS += src_multimedia

src_iMPServer.file = $$PWD/src/iMPServer/iMPServer.pro
src_iMPServer.target = sub-iMPServer
src_iMPServer.depends = src_icorelib src_multimedia

SUBDIRS += src_iMPServer

src_icorelibtest.file = $$PWD/test/icorelibtest.pro
src_icorelibtest.target = sub-icorelibtest
src_icorelibtest.depends = src_icorelib src_multimedia src_iMPServer

SUBDIRS += src_icorelibtest

HEADERS +=
