DEFINES += ___QT___
#LINUX_VERSION = `uname -r`
LINUX_VERSION=3.5.0-23-generic
INCLUDEPATH = /usr/src/linux-headers-$$LINUX_VERSION/include
INCLUDEPATH += /doocs/develop/common/include
INCLUDEPATH += /doocs/develop/common/include/qt_headers_for_lk
HEADERS += \
    ../../../sources_gen/pcie_gen_fnc.h \
    ../../../../../../../../common/include/version_dependence.h \
    ../../../../../../../../common/include/pciegen_io.h \
    ../../../../../../../../common/include/pciegen_io_base.h \
    ../../../../../../../../common/include/pcie_gen_exp.h \
    ../../../../../../../../common/include/includes_gen.h \
    ../../../sources/pcie_gen_fnc.h

SOURCES += \
    ../../../sources_gen/pcie_gen.c \
    ../../../sources_gen/pcie_gen_exp.c \
    ../../../sources/pcie_gen_exp.c \
    ../../../sources/pcie_gen.c
