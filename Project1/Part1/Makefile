# To run "make", you can either set up environment variables via
#		source /opt/iot-devkit/1.7.2/environment-setup-i586-poky-linux
# or set up the following make variables
#


CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SDKTARGETSYSROOT=/opt/iotdk/1.7.2/sysroots/i586-poky-linux
export PATH:=/opt/iotdk/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iotdk/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)

APP = main_part1

obj-m = rbt530_drv.o

all:
	@echo $$PATH
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SDKTARGETSYSROOT)/usr/src/kernel M=$(PWD) modules
	$(CC) -o $(APP) main_part1.c -lpthread --sysroot=$(SDKTARGETSYSROOT)

clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP) 
