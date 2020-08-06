#include <linux/ioctl.h>


extern struct list_head dstack_list;
extern int removedstack_onexit(pid_t pid);



struct output
{
        int d_id;
};

struct dumpmode_t {
         int mode;
};
