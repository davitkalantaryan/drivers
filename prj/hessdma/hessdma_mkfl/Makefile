pcie_gen_drv-objs := pcie_gen_exp.o pcie_gen.o 
obj-m := pcie_gen_drv.o

#timer_drv6.o:
#	cc -c -I/usr/include timer_drv6.o

HOSTNAME2 = $(shell hostname)

SOURCES_DIR=../../../sources
#SOURCES_DIR=
ALL_SOURCES1=$(SOURCES_DIR)/pcie_gen.c $(SOURCES_DIR)/pcie_gen_exp.c $(SOURCES_DIR)/pcie_gen_fnc.h
ALL_SOURCES=pcie_gen.c pcie_gen_exp.c pcie_gen_fnc.h
CUR_DIR=$(PWD)

#cp $(ALL_SOURCES) $(CUR_DIR)/.
#cp $(ALL_SOURCES1) .


all: compile install
default: compile

KVERSION = $(shell uname -r)
copy:
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
install:
	-sudo rmmod pcie_gen_drv
	sudo insmod pcie_gen_drv.ko
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compinstall:
	cp $(ALL_SOURCES1) .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	cp Module.symvers Module.symvers.$(HOSTNAME2).$(KVERSION)
	rm $(ALL_SOURCES)
	-sudo rmmod pcie_gen_drv
	sudo insmod pcie_gen_drv.ko
	sudo chmod 666 /dev/pcie_gen_init
compile:
	cp $(ALL_SOURCES1) .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	cp Module.symvers Module.symvers.$(HOSTNAME2).$(KVERSION)
	rm $(ALL_SOURCES)
clean:
	#rm $(ALL_SOURCES)
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	


	
#EXTRA_CFLAGS	+= -I/usr/include
#EXTRA_CFLAGS	+= -I/doocs/develop/include
EXTRA_CFLAGS	+= -I/doocs/develop/common/include

