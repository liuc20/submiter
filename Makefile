SUBMITER = submiter_test

obj-m += $(SUBMITER).o 

$(SUBMITER)-objs := submiter_interface.o submiter.o

#KERNEL_SOURCE := /usr/src/kernels/linux-3.19/
KERNEL_SOURCE := /lib/modules/`uname -r`/build/

default:
	$(MAKE) -C $(KERNEL_SOURCE) M=`pwd` modules
clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=`pwd` clean
	$(RM) Module.markers modules.order
