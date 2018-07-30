pcie_gen_drv-objs := \
global_pointer_container.o \
mtcagen_dma.o \
mtcagen_gain_rel.o \
mtcagen_irq_all.o \
mtcagen_irq_type1.o \
mtcagen_irq_type2.o \
pci-driver-added-zeuthen.o \
mtcagen_main.o \
mtcagen_ioctl.o \
write_and_inform.o

obj-m := pcie_gen_drv.o

HOSTNAME2 = $(shell hostname)

all: compile
default: compile

KVERSION = $(shell uname -r)
copy:
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
install:
	-sudo rmmod pcie_gen_drv
	sudo insmod pcie_gen_drv.ko
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compinstall:
	#cp ./Module.symvers.$(HOSTNAME2).$(KVERSION).upciedev ./Module.symvers
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	cp Module.symvers Module.symvers.$(HOSTNAME2).$(KVERSION).mtcagen
	-sudo rmmod pcie_gen_drv
	sudo insmod pcie_gen_drv.ko
	sudo chmod 666 /dev/pcie_gen_init
compile:
	cp ./Module.symvers.upciedev ./Module.symvers
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	cp Module.symvers Module.symvers.$(HOSTNAME2).$(KVERSION).mtcagen
clean:
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	


	
#EXTRA_CFLAGS	+= -I/usr/include
#EXTRA_CFLAGS	+= -I/doocs/develop/include
#EXTRA_CFLAGS	+= -I/doocs/develop/common/include

