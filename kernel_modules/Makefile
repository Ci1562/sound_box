#CROSS_COMPILE = aarch64-linux-gnu-
#CC=$(CROSS_COMPILE)gcc

KDIR=/lib/modules/$(shell uname -r)/build
#KDIR=/root/kernel/linux-rpi-6.6.y

#EXTRA_CFLAGS += -D__READL_WRITEL__

obj-m += gpio.o

all:
	make -C $(KDIR) M=$(PWD) modules V=1

clean:
	make -C $(KDIR) M=$(PWD) clean

