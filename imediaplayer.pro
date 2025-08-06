TEMPLATE = subdirs

src_icorelib.file = $$PWD/src/core/icorelib.pro
src_icorelib.target = sub-icorelib

SUBDIRS += src_icorelib

src_gsttools.file = $$PWD/src/gsttools/gsttools.pro
src_gsttools.target = sub-gsttools
src_gsttools.depends = src_icorelib

SUBDIRS += src_gsttools

src_icorelibtest.file = $$PWD/test/icorelibtest.pro
src_icorelibtest.target = sub-icorelibtest
src_icorelibtest.depends = src_icorelib src_gsttools

SUBDIRS += src_icorelibtest
