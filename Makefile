##
# Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
#                           <macan@ncic.ac.cn>
#
# Time-stamp: <2009-06-10 09:50:19 macan>
#
# This is the makefile for SVFS module.
#
# Armed by EMACS.

ifeq ($(PWD),)
PWD := $(shell pwd)
endif

KERNEL_NAME := 2.6.30-rc8
KERNEL_INC := /lib/modules/$(KERNEL_NAME)/build

EXTRA_CFLAGS += -I$(PWD)/include -I$(KERNEL_INC)
EXTRA_CFLAGS += -Wall -O2

ifneq ($(KERNELRELEASE),)

obj-m := svfs_client.o dc.o
dc-objs += mdc/super.o

svfs_client-objs += $(dc-objs)

else

KDIR := /lib/modules/$(KERNEL_NAME)/build

modules:
	echo $(svfs_client-objs)
	make -C $(KDIR) M=`pwd` modules

endif

install:
	scp *.ko root@10.10.111.82:/lib/modules/$(KERNEL_NAME)/kernel/fs/

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module* modules.* .*.o.*

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
