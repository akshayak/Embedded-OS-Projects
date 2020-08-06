/*   A test program for /dev/kbuf
		To run the program, enter "kbuf_tester show" to show the current contend of the buffer.
				enter "kbuf_tester write <input_string> to append the <input_string> into the buffer

*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <pthread.h>
#include <time.h>
#include <sched.h>
#define NOTHREAD 2
#define TREESIZE 10
#define NUMOPS 100
pthread_mutex_t lock1;
pthread_mutex_t lock2;
pthread_mutex_t lock3;
pthread_mutex_t lock4;
int nodeCount1=50,nodeCount2=50;
int fileopCount1=100,fileopCount2=100;


void *threaddev1()
{
	
	int fd, res,key,data,i,devid;
	int read_order=1;
	char skey1[10];
	char sdata1[10];
	char buf[20];
	int lower=1, upper=50;

	/* open devices */
	fd = open("/dev/rbt530_dev1", O_RDWR);
	if (fd < 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}
	//populating the tree with 50 nodes first
	printf("Populating rbt530_dev1 with 50 nodes\n");
	while(nodeCount1>=0)
	{
		pthread_mutex_lock(&lock1); 
		key = (rand() % (upper - lower + 1)) + lower;
		data=key;
		sprintf(buf, "%d%s%d", key, " ", data);
		printf("Writing to rbt530_dev1, key =%d\n",key);
		res = write(fd, buf, strlen(buf)+1);
		nodeCount1--;
		pthread_mutex_unlock(&lock1); 

	}

	//executing 100 random read/write/ioctl operations
	printf("Inovking 100 operations for rbt530_dev1\n");
	i=0;
	lower=1;upper=3;
	while(fileopCount1>=0)
	{	
		pthread_mutex_lock(&lock2); 
		int randnum=(rand() % (upper - lower + 1)) + lower;
		//printf("%d\n",fileopCount1);
		if(randnum==1)
		{
			res=ioctl(fd,read_order,0);
			printf("Invoking ioctl operation for rbt530_dev1\n");
		}
		else if(randnum==2)
		{
			res=read(fd,buf,20);
			printf("Invoking read operation for rbt530_dev1 , read value :%s\n",buf);
		}
		else
		{
			sprintf(buf, "%d%s%d", fileopCount1, " ", fileopCount1); //using counter variable as key
			res=write(fd, buf, strlen(buf)+1);
			printf("Invoking write operation for rbt530_dev1, key=%d\n",fileopCount1);
		}
		sleep(randnum);	
		
		fileopCount1--;
		pthread_mutex_unlock(&lock2); 
	}

	//printf("Res=%d",res);
	/* close devices */
	close(fd);
}


void *threaddev2()
{
	
	int fd, res,key,data,i,devid;
	int read_order=1;
	char skey1[10];
	char sdata1[10];
	char buf[20];
	int lower=1, upper=50;

	/* open devices */
	fd = open("/dev/rbt530_dev2", O_RDWR);
	if (fd < 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}
	//populating the tree with 50 nodes first
	printf("Populating rbt530_dev2 with 50 nodes\n");
	while(nodeCount2>=0)
	{
		pthread_mutex_lock(&lock3); 
		key = (rand() % (upper - lower + 1)) + lower;
		data=key;
		sprintf(buf, "%d%s%d", key, " ", data);
		printf("Writing to rbt530_dev2, key =%d\n",key);
		res = write(fd, buf, strlen(buf)+1);
		nodeCount2--;
		pthread_mutex_unlock(&lock3); 

	}

	//executing 100 random read/write/ioctl operations
	i=0;
	lower=1;upper=3;
	printf("Inovking 100 operations for rbt530_dev2\n");
	while(fileopCount2>=0)
	{	
		pthread_mutex_lock(&lock4); 
		int randnum=(rand() % (upper - lower + 1)) + lower;
		//printf("%d\n",fileopCount2);
		if(randnum==1)
		{
			res=ioctl(fd,read_order,0);
			printf("Invoking ioctl operation for rbt530_dev2\n");
		}
		else if(randnum==2)
		{
			res=read(fd,buf,20);
			printf("Invoking read operation for rbt530_dev2 , read value :%s\n",buf);
		}
		else
		{
			sprintf(buf, "%d%s%d", fileopCount2, " ", fileopCount2);
			res=write(fd, buf, strlen(buf)+1);
			printf("Invoking write operation for rbt530_dev2, key=%d\n",fileopCount2);
		}
		sleep(randnum);	
		
		fileopCount2--;
		pthread_mutex_unlock(&lock4); 
	}

	//printf("Res=%d",res);
	/* close devices */
	close(fd);
}


int main(int argc, char **argv)
{
	pthread_attr_t tattr;
	struct sched_param param;
	pthread_t tid1[2];
	pthread_t tid2[2];
	int i,ret;


	ret = pthread_attr_init (&tattr);

	ret = pthread_attr_getschedparam (&tattr, &param);

	pthread_mutex_init(&lock1, NULL);
	pthread_mutex_init(&lock2, NULL);

    //spwaning 2 threads for device1
	for(i=0;i<NOTHREAD;i++)
	{
		param.sched_priority = i+20;
		pthread_create(&tid1[i], &tattr, threaddev1, NULL);
	}
	//spawning 2 threads for device2
	for(i=0;i<NOTHREAD;i++)
	{
		param.sched_priority = i+21;
		pthread_create(&tid2[i], &tattr, threaddev2, NULL);
	}

	for(i=0;i<NOTHREAD;i++)
	{
		pthread_join(tid1[i],NULL);
		pthread_join(tid2[i],NULL);
	}


	return 0;
}

