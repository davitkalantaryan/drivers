# SIS8300 Makefile
#
# We're just setting the stage here and
# let the Kernel Makefile do the real work.
#

KERNVER := $(shell uname -r)
MODULENAME := sis8300drv

.PHONY: driver

driver:
	make -C /lib/modules/$(KERNVER)/build SUBDIRS=$(shell pwd) modules

install:
	-/sbin/rmmod $(MODULENAME)
	make -C /lib/modules/$(KERNVER)/build SUBDIRS=$(shell pwd) modules_install
	/sbin/depmod -a
	/sbin/modprobe $(MODULENAME)
	cp -f udev-rules/92-struck.rules /etc/udev/rules.d/92-struck.rules

clean:
	-rm -f *~ *.o *.ko .*.o.cmd .*.ko.cmd *.mod.c Module.symvers modules.order
	-rm -f Module.markers
	-rm -r -f .tmp_versions

ifndef src
    src:=$(shell pwd)
endif

SOURCES:=$(notdir $(wildcard $(src)/*.c))
sis8300drv-objs := $(patsubst %.c, %.o, $(filter-out %netbsd.c, $(SOURCES)))

ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS += -DMODULENAME=$(MODULENAME)

obj-m += sis8300drv.o

else

modules: sis8300drv.o

sis8300drv.o: $(sis8300drv-objs)
	$(LD) -r -o $@ $^

endif
