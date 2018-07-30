DEFINES += ___QT___
#LINUX_VERSION = `uname -r`
LINUX_VERSION=3.5.0-23-generic
INCLUDEPATH = /usr/src/linux-headers-$$LINUX_VERSION/include
INCLUDEPATH += /doocs/develop/common/include
INCLUDEPATH += /doocs/develop/common/include/qt_headers_for_lk

HEADERS += \
    ../../../sources/version_dependence_dma.h \
    ../../../sources/includes_dma_test.h \
    ../../../sources/dma_test_exp.h

SOURCES += \
    ../../../sources/dma_test.c \
    ../../../sources/dma_test_exp.c
