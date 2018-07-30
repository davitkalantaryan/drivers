MODULE_NAME = pcie_gen_drv

KVERSION = $(shell uname -r)
HOSTNAME2 = $(shell hostname)
KO_FILES = ../../../ko_files/$(HOSTNAME2)_$(KVERSION)
SOURCES_DIR=../../../sources
CUR_DIR=$(PWD)

$(MODULE_NAME)-objs := \
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

obj-m := $(MODULE_NAME).o

#all: compile install
all: compile
default: compile

copy:
	sudo cp $(KO_FILES)/$(MODULE_NAME).ko /lib/modules/$(KVERSION)/desy_zeuthen/.
install:
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compinstall:
	#cp $(SOURCES_DIR)/*.c .
	#cp $(SOURCES_DIR)/*.h .
	#cp ../../upciedev/upciedev_mkfl/Module.symvers.$(HOSTNAME2).$(KVERSION) ./Module.symvers
	#make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	#cp Module.symvers Module.symvers.$(HOSTNAME2).$(KVERSION)
	#rm -f *.c
	#rm -f *.h
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo chmod 666 /dev/pcie_gen_init
compile:
	cp $(KO_FILES)/Module.symvers.upciedev ./Module.symvers
	cp $(SOURCES_DIR)/*.c .
	cp $(SOURCES_DIR)/*.h .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	mkdir -p $(KO_FILES)
	cp Module.symvers $(KO_FILES)/Module.symvers.$(MODULE_NAME)
	mv $(MODULE_NAME).ko $(KO_FILES)/.
	rm -f *.c
	rm -f *.h
clean:
	rm -f *.c
	rm -f *.h
	rm -f $(KO_FILES)/$(MODULE_NAME).ko
	rm -f $(KO_FILES)/Module.symvers.$(MODULE_NAME)
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	


	
#EXTRA_CFLAGS	+= -I/usr/include
#EXTRA_CFLAGS	+= -I/doocs/develop/include
#EXTRA_CFLAGS	+= -I/doocs/develop/common/include
EXTRA_CFLAGS	+= -DUSE_SEMAPHORE

