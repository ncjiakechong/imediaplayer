TEMPLATE = subdirs

src_icorelib.file = $$PWD/src/core/icorelib.pro
src_icorelib.target = sub-icorelib

SUBDIRS += src_icorelib

src_imultimedia.file = $$PWD/src/multimedia/imultimedia.pro
src_imultimedia.target = sub-imultimedia
src_imultimedia.depends = src_icorelib

SUBDIRS += src_imultimedia

src_test.file = $$PWD/test/test.pro
src_test.target = sub-test
src_test.depends = src_icorelib src_imultimedia

SUBDIRS += src_test

HEADERS +=
