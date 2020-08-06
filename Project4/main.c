#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include "dynamic_dump_stack.h"
#include <pthread.h>

#ifndef GRADING
#define TEST_SYMBOL1 "misc_open"
#define TEST_SYMBOL2 "misc_close"
#define TEST_SYMBOL3 "misc_read"
#define TEST_SYMBOL4 "misc_write"
#define TEST_DRIVERS "/dev/misc_device"
#endif

void *threadfn(void *vargp) 
{ 
   int fd;
   	printf("Opening misc devic from thread..\n");	
	fd = open(TEST_DRIVERS, O_RDWR);
	if (fd < 0 ){
		printf("Can not open misc device file.\n");		
		return 0;
	}
	printf("Opened misc device \n");
	close(fd);

	return NULL;
} 

int main()
{
	int fd;
	struct dumpmode_t ip;
	pthread_t thread_id; 
	int ip_mode;
	int res;
	int dumpid1,dumpid2,dumpid3,dumpid4;
	char buf[20];
	

	printf("Enter the dump mode\n");
	scanf("%d", &ip_mode);

	ip.mode=ip_mode;


	//********Calling insdump for all 4 test symbols**************

	printf("Calling insdump for TEST_SYMBOL1\n");
	dumpid1=syscall(359,TEST_SYMBOL1, &ip); //insdump
	if(dumpid1 <0)
	{
		printf("Error in system call\n");
		return 0;
	}

	printf("Dump id for TEST_SYMBOL1 :%d \n",dumpid1);

	//---------------------------------------------------------------
	dumpid2=syscall(359,TEST_SYMBOL2, &ip); //insdump
	if(dumpid2 <0)
	{
		printf("Error in system call\n");
		return 0;
	}

	printf("Dump id for TEST_SYMBOL2 :%d \n",dumpid2);

	//---------------------------------------------------------------

	dumpid3=syscall(359,TEST_SYMBOL3, &ip); //insdump
	if(dumpid3 <0)
	{
		printf("Error in system call\n");
		return 0;
	}

	printf("Dump id for TEST_SYMBOL3:%d \n",dumpid3);

	//---------------------------------------------------------------

	dumpid4=syscall(359,TEST_SYMBOL4, &ip); //insdump
	if(dumpid4 <0)
	{
		printf("Error in system call\n");
		return 0;
	}

	printf("Dump id for TEST_SYMBOL4:%d\n",dumpid4);

	//******************Calling TEST_DRIVERS from main*****************

	printf("Opening TEST_DRIVERS from main..\n");
	fd = open(TEST_DRIVERS, O_RDWR);
	if (fd < 0 ){
		printf("Can not open misc device file.\n");		
		return 0;
	}
	res = write(fd, buf, strlen(buf)+1);
	res=read(fd,buf,20);
	if(res<0)
		printf("Read/write error \n");
	printf("Opened TEST_DRIVERS from main \n");
	close(fd);

	//*********Spawning a thread****************************************

	printf("Spawning a thread to test same tgid but different pid...\n"); 
    pthread_create(&thread_id, NULL, threadfn, NULL); 
    pthread_join(thread_id, NULL); 
    printf("Thread over\n"); 

    //**********Creating fork*******************************************

    if (fork() == 0) 
    {
    	printf("Opening TEST_DRIVERS from forked process..\n");
    	fd = open(TEST_DRIVERS, O_RDWR);
		if (fd < 0 ){
		printf("Can not open misc device file.\n");		
		return 0;
		}
	
		printf("Opened TEST_DRIVERS from forked process \n");
		close(fd);
		exit(0);
	}
    
	sleep(5);

	//*********Calling rmdump for 2 symbols, the other 2 will be removed on process exit*******
	printf("Calling rmdump for TEST_SYMBOL1 and TEST_SYMBOL2\n");
	syscall(360,dumpid1);
	syscall(360,dumpid2);
	sleep(5);


	//***********Opening the device again, since we have removed 2 krpobes, this should not trigger those 2 dumps******
    	printf("Opening TEST_DRIVERS from main again..\n");
    	fd = open(TEST_DRIVERS, O_RDWR);
		if (fd < 0 ){
		printf("Can not open misc device file.\n");		
		return 0;
		}
	
		printf("Opened TEST_DRIVERS from main again\n");
		close(fd);
		
	//------------------------------------------------


	return 0;


}

