

# List of files included:
Under device folder:
1) Makefile - for cross compilation for device file
2) hcsr_dev.h - header file
3) hcsr_device.c - module to register platform devices
Under driver folder:
1) hcsr_driver1.c - module for platform driver
2) hcsr_dev.h - header file
3) Makefile - for cross compilation of driver

* test.sh - bash script to test the platform device/driver structure


# Execution steps:
1) Execute 'make' command in the host linux system once in device folder and once in driver folder. This produces the hcsr_device.ko and hcsr_driver1.ko respectively.
2) Copy these two files into windows first. (I was unable to connect the ethernet to the linux VM, so it is connected to Windows). Type SCP commands in windows command prompt.
SCP Command Format: scp -P 2222 linux hostpath windows path
example: 
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/part2/device/hcsr_device.ko .
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/part2/driver/hcsr_driver1.ko .
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/part2/test.sh .
This copies the three files into the windows current directory
3) Copy the files from Windows to Galileo board using scp in command prompt
SCP Command format: scp filename username@ip of galileo
example:
-> scp hcsr_device.ko root@192.168.1.5: 
-> scp hcsr_driver1.ko root@192.168.1.5: 
-> scp main root@192.168.1.5: 
4) Now ssh into the galileo board (from windows or linux)
ssh Command format: ssh username@ip address of galileo
example:
ssh root@192.168.1.5 
5) Once the device and driver modules are present in the galileo board, load the modules
insmod hcsr_device.ko
insmod hcsr_driver1.ko
6) Give executable privileges to the test program's executable
chmod 777 test.sh
7) Execute  main
./test.sh

# Results and Output:
* When we insmod the device module, it registers two devices as platform devices
* When we insmod the driver, it probes for the 2 platform devices and creates the devices and the sys attributes
* We can navigate to the sys/class/HCSR/HCSR_0 folder to see 6 attributes created.
* In the test.sh file we first write values to these attributes and then enable the measurement
* After disabling the measurement, we read the distance from the distance attribute.
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------


