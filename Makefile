# Makefile — k4_driver for Android Kernel 4.14.86 ARM64
# -----------------------------------------------------
# HOW TO BUILD (on Linux or WSL2):
#   make KDIR=/path/to/kernel-source

ARCH          := arm64
CROSS_COMPILE := aarch64-linux-gnu-
KDIR          ?= /path/to/kernel-4.14.86

obj-m         += k4_driver.o

all:
	$(MAKE) -C $(KDIR) \
		M=$(shell pwd) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		modules

clean:
	$(MAKE) -C $(KDIR) M=$(shell pwd) clean
	rm -f *.o *.ko *.mod.* *.symvers *.order

# ADB deploy helpers
deploy:
	adb push k4_driver.ko /data/local/tmp/
	adb shell "su -c 'insmod /data/local/tmp/k4_driver.ko'"
	adb shell "su -c 'dmesg | tail -20 | grep K4DRV'"

verify:
	adb shell "su -c 'cat /proc/k4_info'"
	adb shell "su -c 'ls -la /dev/k4drv'"

remove:
	adb shell "su -c 'rmmod k4_driver'"
	adb shell "su -c 'dmesg | tail -5'"
