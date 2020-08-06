
# Generic Netlink Socket and SPI Device Programming

* In this project, we have developed an application program and a kernel module, and used a generic netlink socket to enable the communication between them. 
* There are three types of message to be sent by the application process to the kernel module:
  1. A display pattern for the LED dot matrix,
  2. A configuration command for SPI CS pin, HC-SR04 trigger and echo pins,
  3. A request to measure distance (which is the average of 5 samples collected from HC-SR04).
* The application process then receives a message from the kernel module that contains a measured distance. Based on the measured distance, it determines any updates of display pattern or request a new measurement after a fixed period.
* All IO operations are done asynchronously with the communication operation.
* To display patterns, we have used a simple 8X8 LED matrix which is equipped with a MAX7219 driver.
* In addition to source current for LED display, the driver chip can buffer display data and scan digits using
an internal oscillator.

# List of files included:
1) Makefile - for cross compilation
2) spiDotMatrix.c - spi driver source file
3) main.c - main program source file
4) genl_header.h - header file 
5) Results - contains screenshots of results

# Execution steps:
1) Execute 'make' command in the host linux system. This produces the spiDotMatrix.ko and main executable file
2) Copy these two files into windows first. (I was unable to connect the ethernet to the linux VM, so it is connected to Windows). Type SCP commands in windows command prompt.
SCP Command Format: scp -P 2222 linux hostpath windows path
example: 
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/assignment3/spiDotMatrix.ko .
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/assignment3/main .
This copies the two files into the windows current directory
3) Copy the files from Windows to Galileo board using scp in command prompt
SCP Command format: scp filename username@ip of galileo
example:
-> scp spiDotMatrix.ko root@192.168.1.5: 
-> scp main root@192.168.1.5: 
4) Now ssh into the galileo board (from windows or linux)
ssh Command format: ssh username@ip address of galileo
example:
ssh root@192.168.1.5 
5) Once the module and the main program is present in the galileo board, first we need to remove the existing spidev module
rmmod spidev
load the spidriver module 
insmod spiDotMatrix.ko 
6) Give executable privileges to the main program's executable
chmod 777 main
7) Execute  main
./main
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Results and Output:
-> On execution of main application, the socket gets prepped and sends a message to the kernel with the following values:
1) trigger pin - taken from the macro included in the main file
2) echo pin - taken from the macro included in the main file
3) cs pin - taken from the macro included in the main file. PLEASE PASS THIS VALUE AS 12. I have tested the CS pin for IO12 only.
4) measurement request - to initiate the measurement of HCSR device
5) pattern - initial pattern to light up the led. (Pattern toggling will start once the distance measurement starts. until then the initial pattern is shown in the dot matrix)
5) isInitial - flag to indicate if it is the initial message to kernel. This is used by the kernel to decide if it needs to spawn threads or not)
-> On receiving the initial message from the user space, the kernel module creates 2 threads - one for the LED and one for HCSR device. 
-> Initially the LED is lit up with a butterfly with wings pattern. 
-> When the distance measurement is completed by the HCSR device, the distance is sent to the user space via netlink.
-> When the user space program receives the distance measurement from the HCSR device, it again sends a message to the kernel space with the updated pattern. (butterfly with wings closed).
-> This toggle between the two patters keep going on infinitely.
-> On doing rmmod spiDotMatrix, the threads are killed and the LED is turned off.
-> Please refer to the Results file for screenshots from the output.
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------


