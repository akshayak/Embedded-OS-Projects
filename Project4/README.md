

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


# SAMPLE OUTPUT:

# USER SPACE TEST APPLICATION: 
root@quark:~# ./main
Enter the dump mode
0
Calling insdump for TEST_SYMBOL1
Dump id for TEST_SYMBOL1 :5 
Dump id for TEST_SYMBOL2 :6 
Dump id for TEST_SYMBOL3:7 
Dump id for TEST_SYMBOL4:8
Opening TEST_DRIVERS from main..
Opened TEST_DRIVERS from main 
Spawning a thread to test same tgid but different pid...
Opening misc devic from thread..
Opened misc device 
Thread over
Opening TEST_DRIVERS from forked process..
Opened TEST_DRIVERS from forked process 
Calling rmdump for TEST_SYMBOL1 and TEST_SYMBOL2
Opening TEST_DRIVERS from main again..
Opened TEST_DRIVERS from main again


# KERNEL LOGS WHEN EXECUTING USER SPACE APPLICATION:
[ 3147.408653] ***ADDING KPROBE, PARAMS FROM USER: mode= 0, symbolname=misc_open ***
[ 3147.418457] ***INSERTION OF KPROBE SUCCESSSFUL, dumpid:5 !!***
[ 3147.520354] ***ADDING KPROBE, PARAMS FROM USER: mode= 0, symbolname=misc_close ***
[ 3147.528303] ***INSERTION OF KPROBE SUCCESSSFUL, dumpid:6 !!***
[ 3147.628851] ***ADDING KPROBE, PARAMS FROM USER: mode= 0, symbolname=misc_read ***
[ 3147.636810] ***INSERTION OF KPROBE SUCCESSSFUL, dumpid:7 !!***
[ 3147.740160] ***ADDING KPROBE, PARAMS FROM USER: mode= 0, symbolname=misc_write ***
[ 3147.748111] ***INSERTION OF KPROBE SUCCESSSFUL, dumpid:8 !!***
[ 3147.757638] ***PRE HANDLER of kprobe: p->addr=0xc13023b0, for symbol: misc_open ***
[ 3147.764214] CPU: 0 PID: 348 Comm: main Tainted: G           O   3.19.8-yocto-standard #1
[ 3147.764214] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[ 3147.764214]  ce695600 ce695600 cd259d6c c14539c1 cd259d80 c124ceef ce695600 ce600960
[ 3147.764214]  cd259dc8 cd259d98 c10a27d2 ce695608 cd259dc8 ce600960 c13023b1 cd259db0
[ 3147.764214]  c1027fa4 c13023b0 cd259dc8 00000000 cd7a7280 cd259dc0 c1002964 cd14c100
[ 3147.764214] Call Trace:
[ 3147.764214]  [<c14539c1>] dump_stack+0x16/0x18
[ 3147.764214]  [<c124ceef>] handler_pre+0x5f/0x70
[ 3147.764214]  [<c10a27d2>] aggr_pre_handler+0x32/0x70
[ 3147.764214]  [<c13023b1>] ? misc_open+0x1/0x150
[ 3147.764214]  [<c1027fa4>] kprobe_int3_handler+0xb4/0x130
[ 3147.764214]  [<c13023b0>] ? misc_devnode+0x40/0x40
[ 3147.764214]  [<c1002964>] do_int3+0x44/0xa0
[ 3147.764214]  [<c1457f13>] int3+0x33/0x40
[ 3147.764214]  [<c13023b0>] ? misc_devnode+0x40/0x40
[ 3147.764214]  [<c13023b1>] ? misc_open+0x1/0x150
[ 3147.764214]  [<c111f28c>] ? chrdev_open+0x7c/0x130
[ 3147.764214]  [<c11195df>] do_dentry_open+0x18f/0x2c0
[ 3147.764214]  [<c111f210>] ? cdev_put+0x20/0x20
[ 3147.764214]  [<c111991c>] vfs_open+0x3c/0x50
[ 3147.764214]  [<c1128273>] do_last+0x1f3/0xf00
[ 3147.764214]  [<c1114700>] ? sysfs_slab_alias+0x80/0x80
[ 3147.764214]  [<c1128fe9>] path_openat+0x69/0x530
[ 3147.764214]  [<c112a1e7>] do_filp_open+0x27/0x80
[ 3147.764214]  [<c1134101>] ? __alloc_fd+0x61/0xf0
[ 3147.764214]  [<c1129590>] ? getname_flags+0xa0/0x150
[ 3147.764214]  [<c111ae1a>] do_sys_open+0x10a/0x210
[ 3147.764214]  [<c111af38>] SyS_open+0x18/0x20
[ 3147.764214]  [<c14573c4>] syscall_call+0x7/0x7
[ 3147.764214] post_handler: p->addr=0xc13023b0
[ 3147.956706] Opening misc device
[ 3147.959983] ***PRE HANDLER of kprobe: p->addr=0xd29ce040, for symbol: misc_write ***
[ 3147.960049] CPU: 0 PID: 348 Comm: main Tainted: G           O   3.19.8-yocto-standard #1
[ 3147.960049] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[ 3147.960049]  cd1bec00 cd1bec00 cd259ed4 c14539c1 cd259ee8 c124ceef cd1bec00 ce600a80
[ 3147.960049]  cd259f30 cd259f00 c10a27d2 cd1bec08 cd259f30 ce600a80 d29ce041 cd259f18
[ 3147.960049]  c1027fa4 d29ce040 cd259f30 00000000 d29ce040 cd259f28 c1002964 cd7a7280
[ 3147.960049] Call Trace:
[ 3147.960049]  [<c14539c1>] dump_stack+0x16/0x18
[ 3147.960049]  [<c124ceef>] handler_pre+0x5f/0x70
[ 3147.960049]  [<c10a27d2>] aggr_pre_handler+0x32/0x70
[ 3147.960049]  [<d29ce041>] ? misc_write+0x1/0x20 [rbptestModule]
[ 3147.960049]  [<c1027fa4>] kprobe_int3_handler+0xb4/0x130
[ 3147.960049]  [<d29ce040>] ? misc_open+0x20/0x20 [rbptestModule]
[ 3147.960049]  [<d29ce040>] ? misc_open+0x20/0x20 [rbptestModule]
[ 3147.960049]  [<c1002964>] do_int3+0x44/0xa0
[ 3147.960049]  [<c1457f13>] int3+0x33/0x40
[ 3147.960049]  [<d29ce040>] ? misc_open+0x20/0x20 [rbptestModule]
[ 3147.960049]  [<c111007b>] ? unuse_mm+0x1cb/0x420
[ 3147.960049]  [<d29ce041>] ? misc_write+0x1/0x20 [rbptestModule]
[ 3147.960049]  [<c111bc84>] ? vfs_write+0x94/0x1d0
[ 3147.960049]  [<c111c38f>] SyS_write+0x3f/0x90
[ 3147.960049]  [<c14573c4>] syscall_call+0x7/0x7
[ 3147.960049] post_handler: p->addr=0xd29ce040
[ 3148.090324] Inside misc write !!
[ 3148.100441] ***PRE HANDLER of kprobe: p->addr=0xd29ce060, for symbol: misc_read ***
[ 3148.108172] CPU: 0 PID: 348 Comm: main Tainted: G           O   3.19.8-yocto-standard #1
[ 3148.110055] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[ 3148.110055]  ce695300 ce695300 cd259ec0 c14539c1 cd259ed4 c124ceef ce695300 ce600a20
[ 3148.110055]  cd259f1c cd259eec c10a27d2 ce695308 cd259f1c ce600a20 d29ce061 cd259f04
[ 3148.110055]  c1027fa4 d29ce060 cd259f1c 00000000 bf8104e8 cd259f14 c1002964 d29ce060
[ 3148.110055] Call Trace:
[ 3148.110055]  [<c14539c1>] dump_stack+0x16/0x18
[ 3148.110055]  [<c124ceef>] handler_pre+0x5f/0x70
[ 3148.110055]  [<c10a27d2>] aggr_pre_handler+0x32/0x70
[ 3148.110055]  [<d29ce061>] ? misc_read+0x1/0x20 [rbptestModule]
[ 3148.110055]  [<c1027fa4>] kprobe_int3_handler+0xb4/0x130
[ 3148.110055]  [<d29ce060>] ? misc_write+0x20/0x20 [rbptestModule]
[ 3148.110055]  [<c1002964>] do_int3+0x44/0xa0
[ 3148.110055]  [<d29ce060>] ? misc_write+0x20/0x20 [rbptestModule]
[ 3148.110055]  [<c1457f13>] int3+0x33/0x40
[ 3148.110055]  [<d29ce060>] ? misc_write+0x20/0x20 [rbptestModule]
[ 3148.110055]  [<d29ce061>] ? misc_read+0x1/0x20 [rbptestModule]
[ 3148.110055]  [<c111c154>] ? __vfs_read+0x14/0x60
[ 3148.110055]  [<c111c207>] vfs_read+0x67/0x120
[ 3148.110055]  [<c111c2ff>] SyS_read+0x3f/0x90
[ 3148.110055]  [<c14573c4>] syscall_call+0x7/0x7
[ 3148.110055] post_handler: p->addr=0xd29ce060
[ 3148.230407] Inside misc read !!
[ 3148.235140] ***PRE HANDLER of kprobe: p->addr=0xd29ce000, for symbol: misc_close ***
[ 3148.240316] CPU: 0 PID: 348 Comm: main Tainted: G           O   3.19.8-yocto-standard #1
[ 3148.240316] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[ 3148.240316]  ce695180 ce695180 cd259ec8 c14539c1 cd259edc c124ceef ce695180 ce6002a0
[ 3148.240316]  cd259f24 cd259ef4 c10a27d2 ce695188 cd259f24 ce6002a0 d29ce001 cd259f0c
[ 3148.240316]  c1027fa4 d29ce000 cd259f24 00000000 cd78fe38 cd259f1c c1002964 cd7a7280
[ 3148.240316] Call Trace:
[ 3148.240316]  [<c14539c1>] dump_stack+0x16/0x18
[ 3148.240316]  [<c124ceef>] handler_pre+0x5f/0x70
[ 3148.240316]  [<c10a27d2>] aggr_pre_handler+0x32/0x70
[ 3148.240316]  [<d29ce001>] ? misc_close+0x1/0x20 [rbptestModule]
[ 3148.240316]  [<c1027fa4>] kprobe_int3_handler+0xb4/0x130
[ 3148.240316]  [<d29ce000>] ? 0xd29ce000
[ 3148.240316]  [<c1002964>] do_int3+0x44/0xa0
[ 3148.240316]  [<c1457f13>] int3+0x33/0x40
[ 3148.240316]  [<d29ce000>] ? 0xd29ce000
[ 3148.240316]  [<d29ce001>] ? misc_close+0x1/0x20 [rbptestModule]
[ 3148.240316]  [<c111ceea>] ? __fput+0xaa/0x1c0
[ 3148.240316]  [<c111d038>] ____fput+0x8/0x10
[ 3148.240316]  [<c104cde1>] task_work_run+0x61/0x90
[ 3148.240316]  [<c1002445>] do_notify_resume+0x65/0x70
[ 3148.240316]  [<c145742b>] work_notifysig+0x20/0x25
[ 3148.240316] post_handler: p->addr=0xd29ce000
[ 3148.360526] Closing misc device 
[ 3148.435539] ***PRE HANDLER of kprobe: p->addr=0xc13023b0, for symbol: misc_open ***
[ 3148.440052] post_handler: p->addr=0xc13023b0
[ 3148.449779] Opening misc device
[ 3148.454383] ***PRE HANDLER of kprobe: p->addr=0xd29ce000, for symbol: misc_close ***
[ 3148.462190] post_handler: p->addr=0xd29ce000
[ 3148.466605] Closing misc device 
[ 3148.470482] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 349 ***
[ 3148.477624] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 349 ***
[ 3148.484653] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 349 ***
[ 3148.491666] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 349 ***
[ 3148.511423] ***PRE HANDLER of kprobe: p->addr=0xc13023b0, for symbol: misc_open ***
[ 3148.519149] post_handler: p->addr=0xc13023b0
[ 3148.525809] Opening misc device
[ 3148.531518] ***PRE HANDLER of kprobe: p->addr=0xd29ce000, for symbol: misc_close ***
[ 3148.539327] post_handler: p->addr=0xd29ce000
[ 3148.543771] Closing misc device 
[ 3148.550628] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 350 ***
[ 3148.557770] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 350 ***
[ 3148.564798] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 350 ***
[ 3148.571811] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 350 ***
[ 3153.508839] ***INSIDE rmdump, dumpid= 5 ******DUMPID FOUND for symbol: misc_open***
[ 3153.517439] ***UNREGISTERED KPROBE***
[ 3153.523389] ***INSIDE rmdump, dumpid= 6 ******DUMPID FOUND for symbol: misc_close***
[ 3153.532554] ***UNREGISTERED KPROBE***
[ 3158.537949] Opening misc device
[ 3158.542538] Closing misc device 
[ 3158.545940] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 348 ***
[ 3158.552989] ***KPROBE FOUND FOR SYMBOL misc_write***
[ 3158.560198] ***UNREGISTERED KPROBE***
[ 3158.563914] ***EXIT PROCESS..REMOVING ALL KPROBES FOR THE PROCESS: 348 ***
[ 3158.570969] ***KPROBE FOUND FOR SYMBOL misc_read***
[ 3158.581799] ***UNREGISTERED KPROBE***
