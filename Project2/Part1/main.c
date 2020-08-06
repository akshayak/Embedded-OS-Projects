


#include <string.h>
#include <time.h>
#include "misc_ioctl.h"
#include <sys/fcntl.h> 
#include <sys/stat.h>
#include <sys/ioctl.h>      
#include <unistd.h>     
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
	int fd, res;
	struct ioctl_args args; 
	args.val1=2;
	args.val2=10;
	struct op_buffer *buf;
	int write_param;

	/* open devices */
	fd = open("/dev/HCSR_2", O_RDWR);
	if (fd < 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}
	
	if((res = ioctl(fd,CONFIG_PINS,(unsigned long)&args))!=0)
	{
		printf("CONFIG_PINS ioctl error ");
	}

	args.val1=3; //will run for 5 measurements
	args.val2=5000;
	if((res = ioctl(fd,SET_PARAMETERS,(unsigned long)&args))!=0)
	{
		printf("SET_PARAMETERS ioctl error ");
	}
	//to start the measurement
	write_param=1;
	res = write(fd,&write_param , sizeof(write_param));
	sleep(2);
	//to stop the measurement
	write_param=0;
	res = write(fd,&write_param , sizeof(write_param));


	buf = (struct op_buffer*)malloc(sizeof(struct op_buffer));
	res=read(fd,buf,sizeof(struct op_buffer));
	printf("Time stamp : %llu \n",buf->time_stamp);
	printf("Distance   : %llu cm \n",buf->distance);

	sleep(20);
	printf("Slept for 20 sec, exiting");
	close(fd);

	return 0;
}

