upciedev-objs := upciedev_drv.o pciedev_ufn.o pciedev_probe_exp.o \
	pciedev_remove_exp.o pciedev_rw_exp.o pciedev_ioctl_exp.o \
	criticalregionlock.o
obj-m := upciedev.o 


HOSTNAME2 = $(shell hostname)

CUR_DIR=$(PWD)

all: compile
default: compile

KVERSION = $(shell uname -r)
copy:
	#sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
copysrc:
	./cp_all_files
install:
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compinstall:
	#cp $(ALL_SOURCES1) .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	cp Module.symvers Module.symvers.$(HOSTNAME2).$(KVERSION)
	#rm $(ALL_SOURCES)
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo chmod 666 /dev/pcie_gen_init
compile:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	#rm $(ALL_SOURCES)
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	


	
#EXTRA_CFLAGS	+= -I/usr/include
#EXTRA_CFLAGS	+= -I/doocs/develop/include
#EXTRA_CFLAGS	+= -I/doocs/develop/common/include

