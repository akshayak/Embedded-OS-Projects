
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include<linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include<linux/init.h>
#include<linux/moduleparam.h>
#include <linux/ptrace.h>
#include<linux/thread_info.h>

#define DEVICE_NAME  "rbprobe"  // device name to be created and registered


struct tracedata
{
	pid_t pid;
	kprobe_opcode_t *kpaddr;

};

/* per device structure */
struct rbprobe_dev {
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device*/
	struct tracedata buf; //trace data to be recorded
} *rbprobe_devp;

//extern struct rbt530_dev *rbt;
static struct kprobe kp; //kprobe object
static dev_t rbprobe_dev_number;      /* Allotted device number */
struct class *rbprobe_dev_class;          /* Tie with the device model */
static struct device *rbprobe_dev_device;

static char *user_name = "Dear John";  /* the default user name, can be replaced if a new name is attached in insmod command */

module_param(user_name,charp,0000);	//to get parameter from load.sh script to greet the user


static int     rbprobe_open(struct inode *, struct file *);
static int     rbprobe_release(struct inode *, struct file *);
static ssize_t rbprobe_read(struct file *, char *, size_t, loff_t *);
static ssize_t rbprobe_write(struct file *, const char *, size_t, loff_t *);

/* File operations structure. Defined in linux/fs.h */
static struct file_operations rbprobe_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= rbprobe_open,        /* Open method */
    .release	= rbprobe_release,     /* Release method */
    .write		= rbprobe_write,       /* Write method */
    .read		= rbprobe_read,        /* Read method */
};

//pre handler. Reads address of kprobe, pid of process,TSC and rbtree nodes and stores in kprobe trace buffer
int handler_pre(struct kprobe *p, struct pt_regs *regs)
 {
          
    //struct file *file = (struct file *)regs->di;
    printk("pre_handler: p->addr=0x%p\n",
                  p->addr);
    
    printk("Address of kprobe : %p\n",p->addr);
    printk("pid =%x \n", current->pid);
    printk("Stack pointer : %08lx",regs->sp);
    //printk(KERN_INFO "eax: %08lx   ebx: %08lx   ecx: %08lx   edx: %08lx\n",
    //regs->di, regs->si, regs->dx, regs->cx);

    
    printk("first param=%d\n", (int)regs->di);
  	
  	//rbt = file->private_data;
  	//printk("Device name : %s",rbt->name);
  
  	//storing in trace buffer
    rbprobe_devp->buf.pid=current->pid;
    rbprobe_devp->buf.kpaddr=p->addr;

    //long long tt=(long long)native_read_tsc();
          return 0;
  }
  
  /*kprobe post_handler: called after the probed instruction is executed*/
  void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
  {
          printk("post_handler: p->addr=0x%p\n",
                  p->addr);

    return;
  }

int rbprobe_open(struct inode *inode, struct file *file)
{
	
	struct rbprobe_dev *rbprobe_devp;
	printk("Inside driver open function");

	/* Get the per-device structure that contains this cdev */
	rbprobe_devp = container_of(inode->i_cdev, struct rbprobe_dev, cdev);


	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data = rbprobe_devp;
	printk("\n%s is opening \n", rbprobe_devp->name);
	return 0;
}


int rbprobe_release(struct inode *inode, struct file *file)
{
	struct rbprobe_dev *rbprobe_devp = file->private_data;
	
	printk("\n%s is closing\n", rbprobe_devp->name);
	
	return 0;
}



ssize_t rbprobe_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{

	
	char *srw;
	char *soffset;
	char *sflag;
	char *final=(char *)buf;
	int rw,offset,flag,ret;
	char method_name[30];
	printk("Inside kprobe write : ");

	srw=strsep(&final," ");
	soffset=strsep(&final," ");
	sflag=strsep(&final," ");

	kstrtoint(srw,10,&rw);
	kstrtoint(soffset,10,&offset);
	kstrtoint(sflag,10,&flag);

	printk("rw, offset, flag =%d, %d, %d",rw,offset,flag);

	//register kprobe if flag=1
	if(flag==1)
	{
		if(rw==0) //if rw flag=0 then probe read function
			sprintf(method_name,"rbt530_driver_read");
		else //else probe write function
			sprintf(method_name,"rbt530_driver_write");
		kp.pre_handler = handler_pre;
    	kp.post_handler = handler_post;
		kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name(method_name);
		printk("address : %p",kp.addr);
		kp.offset=offset;
		printk("kprobe address : %p",kp.addr);
    	


    	ret=register_kprobe(&kp);	

    	if (ret < 0) {
			pr_err("register_kprobe failed, returned %d\n", ret);
			return ret;
		}
		pr_info("Planted kprobe at %p\n", kp.addr);
	}
	else if(flag==0) //unregister kprobe if flag=0
	{
		unregister_kprobe(&kp);
		printk("Unregistered kprobe");
	}
	else
		return -1;
	
	return 0;
}


ssize_t rbprobe_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	
	int bytes_read = 0;
	struct rbprobe_dev *rbprobe_devp = file->private_data;
	char address[200];
	char pid[30];
	printk("Inside kprobe read");
	if(rbprobe_devp->buf.pid==0) //if no data is written to the trace buffer yet, return EINVAL
	{
		return -EINVAL;

    }
    else // if trace buffer has data, read and send to user space
    {

    	sprintf(address,"Address of kprobe :%p",rbprobe_devp->buf.kpaddr);
    	sprintf(pid,"PID :%x",rbprobe_devp->buf.pid);

     	printk("Address of kprobe : %p",rbprobe_devp->buf.kpaddr);
    	printk("pid =%x \n", rbprobe_devp->buf.pid);  

    	strcat(address,pid);
    	put_user(address,&buf); 	

    	while (count && address[bytes_read]) {

		put_user(address[bytes_read], buf++);
		count--;
		bytes_read++;
	}
    }
    
	return bytes_read;
    

}




/*
 * rbprobe Driver Initialization
 */
int __init rbprobe_driver_init(void)
{
	int ret;
	printk("Inside init of rbprobe.\n");
	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&rbprobe_dev_number, 0, 1, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	rbprobe_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	rbprobe_devp = kmalloc(sizeof(struct rbprobe_dev), GFP_KERNEL);
		
	if (!rbprobe_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	
	sprintf(rbprobe_devp->name, DEVICE_NAME); //copy DEVICE_NAME to kbuf_devp->name

	/* Connect the file operations with the cdev */
	/* Request I/O region */
	cdev_init(&rbprobe_devp->cdev, &rbprobe_fops); //initialize the cdev
	rbprobe_devp->cdev.owner = THIS_MODULE; 

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&rbprobe_devp->cdev, (rbprobe_dev_number), 1); 

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	rbprobe_dev_device = device_create(rbprobe_dev_class, NULL, MKDEV(MAJOR(rbprobe_dev_number), 0), NULL, DEVICE_NAME);	

	//initializing process id to 0
	rbprobe_devp->buf.pid=(pid_t)0;

	
	printk("rbprobe driver added.\n");
	return 0;
}
/* rbprobe Driver Exit */
void __exit rbprobe_driver_exit(void)
{
	// device_remove_file(kbuf_dev_device, &dev_attr_xxx);
	/* Release the major number */
	unregister_chrdev_region((rbprobe_dev_number), 1);

	/* Destroy device */
	device_destroy (rbprobe_dev_class, MKDEV(MAJOR(rbprobe_dev_number), 0));
	cdev_del(&rbprobe_devp->cdev);
	kfree(rbprobe_devp);
	
	/* Destroy driver_class */
	class_destroy(rbprobe_dev_class);

	printk("rbprobe driver removed.\n");
}

module_init(rbprobe_driver_init);
module_exit(rbprobe_driver_exit);
MODULE_LICENSE("GPL v2");

