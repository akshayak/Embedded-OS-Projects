# RB Tree and Dynamic Probe in Linux Kernel

# Part 1: Accessing a kernel RB tree via device file interface
* In this part, a Linux kernel module has been developed which initiates an empty RB tree in Linux kernel and allows the tree being accessed as a device file.
* The RB tree is implemented in kernel space as a character device “rbt530_dev” and managed by a device driver “rbt530_drv”. 
* When the device driver is installed, the tree “rbt530” is created and a device “rbt530_dev” is added to Linux device file systems. 
* The device driver is implemented as a Linux kernel module and enables the following file operations: open, write, read, Ioctl, release.

# Part 2: Dynamic instrumentation in kernel modules
* In this part, a kernel module is built that creates a character device, named as “RBprobe”. 
* RBprobe uses kprobe API to add and remove dynamic probes in any kernel programs. 
* With the module’s device file interface, a user program can place a kprobe on a specific line of kernel code, access kernel information and variables
* Two RB trees are utilized, “rbt530-1” and “rbt530_2”, and test program reads in kproberequest information from console and then invokes RBprobe device file interface to register/unregister a kprobe at a given location of rbt530_drv module. 
* When the kprobe is hit, the handler retrieves few trace data items in a buffer such that they can be read out via RBprobe module interface. 
* In the scenario, the user input request consists of the location (offset) of a source line of code on the execution path of read and write functions of rbt530_drv. 
* The buffer is with a fixed size and holds one set of trace data, i.e. any old trace data will be overwritten when new trace data is generated. 
* The trace data items to be collected by kprobe handler include: the address of the kprobe, the pid of the running process that hitsthe probe, time stamp (x86 TSC), and all rb_object objects traversed in the RB tree while performing thecorresponding functions.
