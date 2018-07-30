DEFINES += ___QT___
#LINUX_VERSION = `uname -r`
LINUX_VERSION=3.13.0-24-generic
INCLUDEPATH = /usr/src/linux-headers-$$LINUX_VERSION/include
INCLUDEPATH += /doocs/develop/common/include
INCLUDEPATH += /doocs/develop/common/include/qt_headers_for_lk

#INCLUDEPATH +=/usr/src/linux-source-3.13.0/linux-source-3.13.0/include
#INCLUDEPATH +=/doocs/develop/common/include

HEADERS += \
    ../../../sources_gen/pcie_gen_fnc.h \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/locking/mutex.h \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/locking/mutex-debug.h \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/include/linux/semaphore.h \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/include/linux/jiffies.h \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/include/linux/spinlock.h \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/include/linux/kprobes.h

SOURCES += \
    ../../../sources_gen/pcie_gen_exp.c \
    ../../../sources_gen/pcie_gen.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/locking/semaphore.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/locking/mutex.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/locking/mutex-debug.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/time/timeconv.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/time/jiffies.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/time/timekeeping.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/time.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/fs/fscache/operation.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/locking/spinlock.c \
    ../../../../../../../../../../../../../../usr/src/linux-source-3.13.0/linux-source-3.13.0/kernel/events/uprobes.c
