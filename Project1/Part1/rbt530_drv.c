

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/rbtree.h>
#include <linux/jiffies.h>
#include <linux/errno.h>

#include<linux/init.h>
#include<linux/moduleparam.h>

#define DEVICE_NAME                 "rbt530_dev"  // device name to be created and registered
#define MAX_DEVICES	2 //to create two devices

// node of an rb tree
struct rb_object {
int key;
int data ;
struct rb_node node;
}rb_object_t;

/* per device structure */
struct rbt530_dev {
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device*/
	struct rb_root root; //root pointer to the rbtree
	int read_order; // to read the rb tree in either ascending or descing order, set by ioctl call
	struct rb_node *cur_node; // used to remember the curent node to retrieve the next/prev node for the read operation.
} *rbt530;

static struct rbt530_dev *rbt;
EXPORT_SYMBOL(rbt);

static dev_t rbt530_dev_number;      /* Allotted device number */
struct class *rbt530_dev_class;          /* Tie with the device model */
struct rbt530_dev *rbt_dev[MAX_DEVICES];

static char *user_name = " ";  /* the default user name, can be replaced if a new name is attached in insmod command */

module_param(user_name,charp,0000);	//to get parameter from load.sh script to greet the user


static int     rbt530_driver_open(struct inode *, struct file *);
static int     rbt530_driver_release(struct inode *, struct file *);
noinline static ssize_t rbt530_driver_write(struct file *, const char *, size_t, loff_t *);
noinline static ssize_t rbt530_driver_read(struct file *, char *, size_t, loff_t *);
static long rbt530_driver_ioctl(struct file *, unsigned int, unsigned long);

/* File operations structure. Defined in linux/fs.h */
static struct file_operations rbt530_dev_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= rbt530_driver_open,        /* Open method */
    .release	= rbt530_driver_release,     /* Release method */
    .write		= rbt530_driver_write,       /* Write method */
    .read		= rbt530_driver_read,        /* Read method */
    .unlocked_ioctl = rbt530_driver_ioctl,
};


/*
* Open rbt530 driver
*/
int rbt530_driver_open(struct inode *inode, struct file *file)
{
	struct rbt530_dev *rbt530;
	printk("Inside driver open function");
	

	/* Get the per-device structure that contains this cdev */
	rbt530 = container_of(inode->i_cdev, struct rbt530_dev, cdev);


	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data = rbt530;
	printk("\n%s is opening \n", rbt530->name);
	return 0;
}

/*
 * Release rbt530 driver
 */
int rbt530_driver_release(struct inode *inode, struct file *file)
{
	struct rbt530_dev *rbt530 = file->private_data;
	printk("Inside driver release function");
	
	
	printk("\n%s is closing\n", rbt530->name);
	return 0;
}

//function to search a node in the rbtree
struct rb_object *nodeSearch(struct rb_root *root, int key)
  {
  	struct rb_node *cur = root->rb_node;
  	while (cur) {
  		struct rb_object *rb_object_t = container_of(cur, struct rb_object, node);
		
		if (key < rb_object_t->key)
  			cur = cur->rb_left;
		else if (key > rb_object_t->key)
  			cur = cur->rb_right;
		else
  			return rb_object_t;
	}
	return NULL;
  }



/*
 * Write to rbt530 driver
 */
noinline ssize_t  rbt530_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{

	char *skey;
	char *sdata;
	char *final=(char *)buf;
	int key,data;
	struct rbt530_dev *rbt530 = file->private_data;
	printk("Inside write function : %s",buf);

	skey=strsep(&final," ");
	sdata=strsep(&final," ");

	kstrtoint(skey,10,&key);
	kstrtoint(sdata,10,&data);

	printk("Key, Data =%d, %d",key,data);

	if(data!=0) //if data not 0 then add/replace the node
	{
		struct rb_object *old_object = nodeSearch(&rbt530->root, key);
		//creating new object with the key and data
		struct rb_object *rb_object_t=kmalloc(sizeof(struct rb_object),GFP_KERNEL);
		struct rb_node *node=kmalloc(sizeof(struct rb_node),GFP_KERNEL); 

		rb_object_t-> key=key;
		rb_object_t-> data=data;
		rb_object_t->node=*node;

		if(old_object) //node with key already exists -> replace with new node
		{
			rb_replace_node(&old_object->node, &rb_object_t->node, &rbt530->root);
			printk("Node replaced");
		}
		else //node with key does not already exist -> create a new node and insert to tree
		{

			struct rb_node **new = &(rbt530->root.rb_node), *parent = NULL;
  	
  			while (*new) 
  			{
  				struct rb_object *this = container_of(*new, struct rb_object, node);
  		
				parent = *new;
  				if (key > this->key)
  					new = &((*new)->rb_right);
  				else if (key < this->key)
  					new = &((*new)->rb_left);
  				else
  					return -1;
  			}
  	
  			rb_link_node(&rb_object_t->node, parent, new);
   			rb_insert_color(&rb_object_t->node, &rbt530->root);
   			printk("Inserted node with key=%d\n",rb_object_t->key); 
   		}
   }
   else //data =0
   {
   		//Search for the node with the given key
   		struct rb_object *rb_object_t = nodeSearch(&rbt530->root, key);
   		if(rb_object_t)
   		{
   			//Delete the node if found
       		rb_erase(&rb_object_t->node , &rbt530->root);
       		kfree(rb_object_t);  

   			printk("Key found and deleted: %d",key);
   		}
   		else
   			printk("Key to be deleted not found");

  	}
	
  	printk("Exiting write function");

	return 0;
}

EXPORT_SYMBOL(rbt530_driver_write);
/*
 * Read from rbt530 driver
 */
noinline ssize_t rbt530_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	
	char retVal[10];
	struct rbt530_dev *rbt530 = file->private_data;
	struct rb_object *rootdata, *cur_object ;
	struct rb_node *cur;
	printk("Inside read function: Read order= %d",rbt530->read_order);


	
	if(rbt530->root.rb_node) //if tree is not null, do read operations
	{
	rootdata = container_of( rbt530->root.rb_node, struct rb_object, node);
 	printk("Root node key: %d data:%d\n", rootdata->key,rootdata->data);

 	
 	if(rbt530->cur_node) //current becomes where we left off from last read
 	{
 		cur=rbt530->cur_node;
 		if(rbt530->read_order==0) //if read order is 0, then read in ascending
 			cur=rb_next(cur);
 		else //is read order is 1, read in descending
 			cur=rb_prev(cur);
 	}
 	else //first read
 	{
 		if(rbt530->read_order==0) //if read order =0, then start from least node
 			cur=rb_first(&rbt530->root);
 		else //if read order is 1, then start from the highest node
 			cur=rb_last(&rbt530->root);
 	}

 	if(cur) //if next/prev is not null, return the node
 	{
 		rbt530->cur_node=cur;

 		cur_object = container_of( cur, struct rb_object, node);
 		printk("Returned node : %d \n", cur_object->key);
 		sprintf(retVal,"%d",cur_object->key);
 		put_user(retVal,&buf);
 	}	

 	else // next/prev is null so we return -1 
 	{
 		return -EINVAL;
 	}

 	}
 	else //tree is null so we return -1
 	{
 		return -EINVAL;
 	}
 	return 0;
 	/*while (cur) 
 	{
  		cur_object = container_of( cur, struct rb_object, node);
  		printk("Key: %d data:%d\n", cur_object->key,cur_object->data);
  		cur = rb_next(cur);
  		counter++;
 	}*/
}

EXPORT_SYMBOL(rbt530_driver_read);

long rbt530_driver_ioctl(struct file *file, unsigned int order, unsigned long arg)
{
	struct rbt530_dev *rbt530 = file->private_data;
	if(order==0 || order ==1) //if argument passed to ioctl is 0 or 1 set it as file order
	{
		rbt530->read_order=order;
		printk("Read order set to : %d\n",order);
	}
	else //if argument is not 0 or 1, return EINVAL
	{
		return -EINVAL;
	}

	return 0;
}

/*
 * rbt530 driver Initialization
 */
int __init rbt530_driver_init(void)
{
	int ret,major;
	int i;
	dev_t this_device;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&rbt530_dev_number, 0, MAX_DEVICES, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}
	
	major = MAJOR(rbt530_dev_number);
	
	/* Populate sysfs entries */
	rbt530_dev_class = class_create(THIS_MODULE, "rbt530_class");

	//init for multiple devices
	for(i=0;i<MAX_DEVICES;i++)
	{
		char dev_name[20];
		snprintf(dev_name,20,"rbt530_dev%d", i+1);
		printk("Device name : %s",dev_name);

		this_device = MKDEV(major, i);

		/* Allocate memory for the per-device structure */
		rbt_dev[i] = kmalloc(sizeof(struct rbt530_dev), GFP_KERNEL);
		if (!rbt_dev[i]) {
			printk("Bad Kmalloc\n"); return -ENOMEM;
		}
	
		sprintf(rbt_dev[i]->name, dev_name); 
		printk("Device name again: %s",rbt_dev[i]->name);

		/* Connect the file operations with the cdev */
		/* Request I/O region */
	
		cdev_init(&rbt_dev[i]->cdev, &rbt530_dev_fops); //initialize the cdev
		rbt_dev[i]->cdev.owner = THIS_MODULE; 

		/* Connect the major/minor number to the cdev */
	
		ret = cdev_add(&rbt_dev[i]->cdev, this_device, 1); 

		if (ret) {
			printk("Bad cdev\n");
			return ret;
		}

		printk("Creating device : %s",rbt_dev[i]->name);
		//create devices under /dev
	
		device_create(rbt530_dev_class, NULL, this_device, NULL, dev_name);		

		rbt_dev[i]->root=RB_ROOT;
		rbt_dev[i]->read_order=0;
	}

	printk("rbt530 driver initialized.\n");
	return 0;
}
/* rbt530 driver Exit */
void __exit rbt530_driver_exit(void)
{
	int i;
	int major = MAJOR(rbt530_dev_number);
	dev_t this_device;
	for(i=0;i<MAX_DEVICES;i++)
	{
		this_device = MKDEV(major, i);
		cdev_del(&rbt_dev[i]->cdev);
		printk("Destroying device : %s",rbt_dev[i]->name);
		/* Destroy device */
		device_destroy (rbt530_dev_class, this_device);
		
		kfree(rbt_dev[i]);
	}
	/* Destroy driver_class */
	class_destroy(rbt530_dev_class);
	/* Release the major number */
	unregister_chrdev_region((rbt530_dev_number), MAX_DEVICES);

	printk("rbt530 driver removed.\n");
}

module_init(rbt530_driver_init);
module_exit(rbt530_driver_exit);
MODULE_LICENSE("GPL v2");

