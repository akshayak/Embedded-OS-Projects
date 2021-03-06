

# List of files included:
1) Makefile - for cross compilation
2) RBProbe.c - device driver source file
3) main_part2.c - main program source file

# Execution steps:
1) Execute 'make' command in the host linux system. This produces the RBProbe.ko and main_part2 executable file
2) Copy these two files into windows first. (I was unable to connect the ethernet to the linux VM, so it is connected to Windows). Type SCP commands in windows command prompt.
SCP Command Format: scp -P 2222 linux hostpath windows path
example: 
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/rbtree/RBProbe.ko .
-> scp -P 2222 akshaya@localhost:/home/akshaya/EOSI/rbtree/main_part2 .
This copies the two files into the windows current directory
3) Copy the files from Windows to Galileo board using scp in command prompt
SCP Command format: scp filename username@ip of galileo
example:
-> scp RBProbe.ko root@192.168.1.5: 
-> scp main_part2 root@192.168.1.5: 
4) Now ssh into the galileo board (from windows or linux)
ssh Command format: ssh username@ip address of galileo
example:
ssh root@192.168.1.5 
5) Once the module and the main program is present in the galileo board, load the module
insmod RBProbe.ko
6) Give executable privileges to the main program's executable
chmod 777 main_part2
7) Execute main_part2
./main_part2

# Results and Output:
* The main program starts by getting user input for offset to place the kprobe. 1 is passed for registering kprobe
* The main program first registers 4 threads with 2 threads each to populate for each rbtree driver, and an additional thread to register a kprobe at the write function of the rbtree530 driver.
* Each rbtree is first populated with 50 objects using write opeation with random integers as keys
* After initial population, 100 operations of read, write and ioctl are triggered randomly to work on the same populated tree, for both trees.
* While the write oprations take place, the kprobe is activated and the pre handler stores the pid and kprobe address 
* After the threads complete, the kprobe is read and displayed.
* After reading the kprobe details , the kprobe is unregistered



