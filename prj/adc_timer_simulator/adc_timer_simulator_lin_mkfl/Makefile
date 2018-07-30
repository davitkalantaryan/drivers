adc_timer_simulatorzn-objs := \
main_adc_timer_simulator.o 

obj-m := adc_timer_simulatorzn.o

KVERSION = $(shell uname -r)
HOSTNAME2 = $(shell hostname)

SOURCES_DIR=../../../sources

#all: compile install
all: compile
default: compile


copy:
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
install:
	-sudo rmmod pcie_gen_drv
	sudo insmod pcie_gen_drv.ko
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compinstall:
	cp ../../pcie_gen/pcie_gen_mkfl/Module.symvers.$(HOSTNAME2).$(KVERSION) ./Module.symvers
	cp $(SOURCES_DIR)/*.c .
	cp $(SOURCES_DIR)/*.h .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	rm -f *.c
	rm -f *.h
	#-sudo rmmod pcie_gen_drv
	#sudo insmod pcie_gen_drv.ko
	#sudo chmod 666 /dev/pcie_gen_init
compile:
	cp ../../pcie_gen/pcie_gen_mkfl/Module.symvers.$(HOSTNAME2).$(KVERSION) ./Module.symvers
	cp $(SOURCES_DIR)/*.c .
	cp $(SOURCES_DIR)/*.h .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	rm -f *.c
	rm -f *.h
clean:
	rm -f *.c
	rm -f *.h
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	


	
#EXTRA_CFLAGS	+= -I/usr/include
#EXTRA_CFLAGS	+= -I/doocs/develop/include
#EXTRA_CFLAGS	+= -I/doocs/develop/common/include

