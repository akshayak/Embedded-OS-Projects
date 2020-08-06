#include <linux/ioctl.h>

#define MAGIC_NUMBER 'K'
#define CONFIG_PINS _IOWR(MAGIC_NUMBER, 0, struct ioctl_args)
#define SET_PARAMETERS _IOWR(MAGIC_NUMBER, 1, struct ioctl_args)


typedef struct ioctl_args
{
	int val1;
	int val2;
} *i_args;

struct op_buffer
{
	unsigned long long int time_stamp;
	unsigned long long int distance;
};