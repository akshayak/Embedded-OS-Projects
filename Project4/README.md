# A Dynamic Stack Dumping in Linux Kernel

* Two new syscalls to insert and remove dump_stack in the execution path of kernel programs. In other words, the insertion and deletion of dump_stack is done dynamically, without re-building the kernel
* The syscall insdump inserts dump_stack operation at the symbol’s address which enables back-tracing of the process stack when program execution hits the address. 
* The process (normal or light-weight) which makes the syscall is regarded as the owner of the dump_stack operation and is responsible to make a call to remove the dump_stack operation (by calling the 2nd syscall rmdump). 
* The dump_stack operation is enabled for the owner process if dumpmode is 0, or for the processes that have the same parent process as the owner if dumpmode is 1. Otherwise, it is enabled for any processes.
# LIST OF FILES INCLUDED:
1) Makefile - for cross compilation of user application
2) assignment4.patch - patch file for the kernel with the system call changes included
3) main.c - main program source file to test the system call
4) dynamic_dump_stack.h - header file 


# EXECUTION STEPS:
1) cd into the kernel directory of the fresh kernel copy. Patch the fresh version of the kernel with the patch file using the command:
patch -p1 < ../assignment4.patch
2) In order to enable the insmod and rmmod system calls, add the following to kernel/.config file
CONFIG_DYNAMIC_DUMP_STACK=y
3) After the kernel gets patched, build the kernel after setting the Path variable:
export PATH=/opt/iotdk/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$PATH
ARCH=x86 LOCALVERSION= CROSS_COMPILE=i586-poky-linux- make -j4
4) After the kernel gets compiled, copy kernel/arch/x86/boot/bzImage on to the sd card and then reboot the galileo board.
Now the galileo board has the patched kernel with new system calls included.
5) Execute 'make' command in the host linux system. This produces the and main executable file
6) Copy the main file into windows first. (I was unable to connect the ethernet to the linux VM, so it is connected to Windows). Type SCP commands in windows command prompt.
SCP Command Format: scp -P 2222 linux hostpath windows path
example: 
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/assignment3/main .
This copies the main file into the windows current directory
7) Copy the main file from Windows to Galileo board using scp in command prompt
SCP Command format: scp filename username@ip of galileo
example:
-> scp main root@192.168.1.5: 
8) Now ssh into the galileo board (from windows or linux)
ssh Command format: ssh username@ip address of galileo
example:
ssh root@192.168.1.5 
9) Once the patched kernel and the main program is present in the galileo board, give executable privileges to the main program's executable
chmod 777 main
10) Execute  main
./main

# RESULTS AND OUTPUT:
1) On executing main, it first asks for the dump mode. We need to input the dump mode.
2) For testing purpose I have created a separate module that creates a device called "/dev/misc_device" which I have given as the value for TEST_DRIVERS.
I have tested the insdump and rmdump on the 4 methods of this module : misc_open, misc_close, misc_read, misc_write. These 4 are the values of my TEST_SYMBOL1, TEST_SYMBOL2, TEST_SYMBOL3 and TEST_SYMBOL4 respectively.
In place of these symbols, the testing symbols can be introduced.
3) In the test application, first insdump will be called for all 4 test symbols. Then the device will be opened and closed from  the same process triggering the dump_stack() irrespective of the mode given
4) Then a thread is spawned that has the same tgid as the owner process but a different pid. the device is opened and closed from this thread. If the dump mode is input as 0, the dump_stack() will not be triggered for this action. If the dump_mode is 1 or above, then the dump_stack() will be called for this thread action.
5) A child process is forked from the main process and has a different process id than the main process. IF the dump mode is 0 or 1, then dump_stack() will not be triggered by this child process. If the dump mode input is >1, then the dump_stack() will be triggered for this child process.
6) After testing for owner process, thread process and fork process, the application calls rmdump for TEST_SYMBOL1 and TEST_SYMBOL2 only.
7) The kprobe associated with rest of the two test symbols TEST_SYMBOL3 and TEST_SYMBOL4 will be automatically removed when the process exits.

