##
# Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
#                           <macan@ncic.ac.cn>
#
# Time-stamp: <2009-06-11 08:52:27 macan>
#
# This is the makefile for SVFS module.
#
# Armed by EMACS.

ifeq ($(PWD),)
PWD := $(shell pwd)
endif

KERNEL_NAME := 2.6.30-rc8
KERNEL_INC := /lib/modules/$(KERNEL_NAME)/build

EXTRA_CFLAGS += -I$(PWD)/include -I$(KERNEL_INC) -DMDC_TRACING_EXTERNAL
EXTRA_CFLAGS += -Wall -O2

MDC := mdc
COMP := comp

ifneq ($(KERNELRELEASE),)

obj-m := svfs_client.o
mdc-objs += $(MDC)/super.o $(MDC)/inode.o $(MDC)/namei.o $(MDC)/fsync.o \
			$(MDC)/dir.o $(MDC)/ialloc.o $(MDC)/mdc.o

svfs_client-objs += $(COMP)/client.o
svfs_client-objs += $(mdc-objs)

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
	rm -rf $(MDC)/*.o $(MDC)/.*.cmd
	rm -rf $(COMP)/*.o $(COMP)/.*.cmd

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
