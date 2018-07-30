MODULE_NAME = sis8300zn

KVERSION = $(shell uname -r)
HOSTNAME2 = $(shell hostname)
KO_FILES = ../../../ko_files/$(HOSTNAME2)_$(KVERSION)
SOURCES_DIR=../../../sources
CUR_DIR=$(PWD)

$(MODULE_NAME)-objs := \
	sis8300_main.o 

obj-m := $(MODULE_NAME).o

#all: compile install
all: compile
default: compile


copy:
	sudo cp $(KO_FILES)/$(MODULE_NAME).ko /lib/modules/$(KVERSION)/desy_zeuthen/.
insert:
	sudo insmod $(KO_FILES)/$(MODULE_NAME).ko
install:
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compinstall:
	#cp ../../pcie_gen/pcie_gen_mkfl/Module.symvers.$(HOSTNAME2).$(KVERSION) ./Module.symvers
	#cp $(SOURCES_DIR)/*.c .
	#cp $(SOURCES_DIR)/*.h .
	#make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	#rm -f *.c
	#rm -f *.h
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo chmod 666 /dev/pcie_gen_init
compile:
	cp $(KO_FILES)/Module.symvers.pcie_gen_drv ./Module.symvers
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

