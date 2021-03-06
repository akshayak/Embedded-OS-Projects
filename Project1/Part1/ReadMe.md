

# List of files included:
1) Makefile - for cross compilation
2) rbt530_drv.c - device driver source file
3) main_part1.c - main program source file

# Execution steps:
1) Execute 'make' command in the host linux system. This produces the rbt530_drv.ko and main_part1 executable file
2) Copy these two files into windows first. (I was unable to connect the ethernet to the linux VM, so it is connected to Windows). Type SCP commands in windows command prompt.
SCP Command Format: scp -P 2222 linux hostpath windows path
example: 
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/rbtree/rbt530_drv.ko .
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/rbtree/main_part1 .
This copies the two files into the windows current directory
3) Copy the files from Windows to Galileo board using scp in command prompt
SCP Command format: scp filename username@ip of galileo
example:
-> scp rbt530_drv.ko root@192.168.1.5: 
-> scp main_part1 root@192.168.1.5: 
4) Now ssh into the galileo board (from windows or linux)
ssh Command format: ssh username@ip address of galileo
example:
ssh root@192.168.1.5 
5) Once the module and the main program is present in the galileo board, load the module
insmod rbt530_drv.ko
6) Give executable privileges to the main program's executable
chmod 777 main_part1
7) Execute main_part1
./main_part1

# Results and Output:
* The main program first registers 4 threads with 2 threads each to populate for each rbtree driver
* Each rbtree is first populated with 50 objects using write opeation with random integers as keys
* After initial population, 100 operations of read, write and ioctl are triggered randomly to work on the same populated tree, for both trees.
* While write happens, the written key is printed, while read happens, the read node value is printed.
* Each thread is synchronized with the other thread populating the same tree. This synchronization is done through mutex locks



