pcie_gen_drv-objs := pcie_gen_exp.o pcie_gen.o 
obj-m := pcie_gen_drv.o

#timer_drv6.o:
#	cc -c -I/usr/include timer_drv6.o


KVERSION = $(shell uname -r)
all:
	#make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules
	#make -C ../sources_gen
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	#test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) clean
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	
EXTRA_CFLAGS	+= -I/usr/include

