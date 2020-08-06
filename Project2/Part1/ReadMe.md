

# List of files included:
1) Makefile - for cross compilation
2) miscdriver1.c - misc device driver source file
3) main.c - main program source file
4) misc_ioctl.h - header file 

# Execution steps:
1) Execute 'make' command in the host linux system. This produces the miscdriver1.ko and main executable file
2) Copy these two files into windows first. (I was unable to connect the ethernet to the linux VM, so it is connected to Windows). Type SCP commands in windows command prompt.
SCP Command Format: scp -P 2222 linux hostpath windows path
example: 
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/miscdriver/miscdriver1.ko .
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/miscdriver/main .
This copies the two files into the windows current directory
3) Copy the files from Windows to Galileo board using scp in command prompt
SCP Command format: scp filename username@ip of galileo
example:
-> scp miscdriver1.ko root@192.168.1.5: 
-> scp main root@192.168.1.5: 
4) Now ssh into the galileo board (from windows or linux)
ssh Command format: ssh username@ip address of galileo
example:
ssh root@192.168.1.5 
5) Once the module and the main program is present in the galileo board, load the module with number of misc devices as param
insmod miscdriver1.ko nodevices=5
6) Give executable privileges to the main program's executable
chmod 777 main
7) Execute  main
./main

# Results and Output:
* The main program first opens a device file
* First we configure the device using 2 ioctl calls, one call for configuring pins and one for configuring m value and delta values
* Then we do a write call using parameter=1. This initiates the measurement for the device.
* To stop the measurement, we make another write call this time with param as 0. This stops the measurement.
* Then we make a read call that returns the time and distance.



