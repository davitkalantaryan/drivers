dma_test0_drv-objs := dma_test.o 
obj-m := dma_test0_drv.o


SOURCES_DIR=../../../sources
#SOURCES_DIR=
ALL_SOURCES1=$(SOURCES_DIR)/dma_test.c $(SOURCES_DIR)/dma_test_exp.c $(SOURCES_DIR)/dma_test_exp.h $(SOURCES_DIR)/includes_dma_test.h \
$(SOURCES_DIR)/version_dependence_dma.h $(SOURCES_DIR)/dma_test_kernel_user.h
ALL_SOURCES=dma_test.c dma_test_exp.c dma_test_exp.h includes_dma_test.h version_dependence_dma.h
CUR_DIR=$(PWD)

#cp $(ALL_SOURCES) $(CUR_DIR)/.
#cp $(ALL_SOURCES1) .


all: compile install
default: compile

KVERSION = $(shell uname -r)
copy:
	sudo cp pcie_gen_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
install:
	-sudo rmmod dma_test0_drv
	sudo insmod dma_test0_drv.ko
	sudo cp dma_test0_drv.ko /lib/modules/$(KVERSION)/desy_zeuthen/.
compile:
	cp $(ALL_SOURCES1) .
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
	#cp Module.symvers Module.symvers.$(KVERSION)
	rm $(ALL_SOURCES)
clean:
	#rm $(ALL_SOURCES)
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	


	
#EXTRA_CFLAGS	+= -I/usr/include
#EXTRA_CFLAGS	+= -I/doocs/develop/include
#EXTRA_CFLAGS	+= -I/doocs/develop/common/include

