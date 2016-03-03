/* ----------------------------------------------- DRIVER squeue -------------------------------------------------
 
 Basic driver example to show skelton methods for several file operations.
 
 ----------------------------------------------------------------------------------------------------------------*/

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
#include <linux/semaphore.h>
#include<linux/init.h>
#include<linux/moduleparam.h>

#define DEVICE_NAME_1                 "bus_in_q"    // device name to be created and registered
#define DEVICE_NAME_2                 "bus_out_q1"  // device name to be created and registered
#define DEVICE_NAME_3                 "bus_out_q2"  // device name to be created and registered
#define DEVICE_NAME_4                 "bus_out_q3"  // device name to be created and registered
#define MAX_QUEUE_SIZE                10
#define MAX_MSG_SIZE                  112
/* per device structure */
struct inqueue {
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device*/
	char msgPtr[MAX_MSG_SIZE*MAX_QUEUE_SIZE];   /* array of msg ptrs */
	int queuesize;                  /*total number of elements currently in queue*/
        int crntPtr;                    /* crnt ptr no */
        struct semaphore sema;
} ;
/* message structure */
struct msg
{
   int sid;               /* Source Id */
   int did;               /* Destination Id */
   int mid;               /* Message Id */
   unsigned long enque_time;        /* Enqueue time stamp*/
   unsigned long accum_time;        /* accumulated time in queue */
   char msg_buffer[80];   /* user define msg buff*/
} ;

static dev_t inqueue_dev_number[4];      /* Allotted device number */
struct class *inqueue_dev_class[4];          /* Tie with the device model */
static struct device *inqueue_dev_device[4];
static struct inqueue *inqueuep_global[4];

unsigned long time_stamp_counter(void)
{
   unsigned long lo, hi;
   
   asm( "rdtsc" : "=a" (lo), "=d" (hi) ); 
   return( lo | (hi << 32) );
}
/*
* Open squeue driver
*/
int inqueue_driver_open(struct inode *inode, struct file *file)
{
	struct inqueue *inqueuep;

	/* Get the per-device structure that contains this cdev */
	inqueuep = container_of(inode->i_cdev, struct inqueue, cdev);


	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data = inqueuep;
//	printk("\n%s is openning \n", inqueuep->name);
	return 0;
}

/*
 * Release squeue driver
 */
int inqueue_driver_release(struct inode *inode, struct file *file)
{
	//struct inqueue *inqueuep = file->private_data;
	
//	printk("\n%s is closing\n", inqueuep->name);
	
	return 0;
}

/*
 * Write to squeue driver
 */
ssize_t inqueue_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{
	struct inqueue *inqueuep = file->private_data;
        char *ptr;
        int tempVar = 0;
        struct msg *message;
 
        down(&inqueuep->sema);
	if(inqueuep->queuesize == MAX_QUEUE_SIZE)
        {
//          printk("\n%s is Full\n", inqueuep->name);
          up(&inqueuep->sema);
          return -1;
        }
        //printk("\n [driver] write request for queue:%s\n",inqueuep->name);
        //ptr = kmalloc(count, GFP_KERNEL);
        if(inqueuep->queuesize != 0)
        {
          inqueuep->crntPtr++;
        }
        else
        {
          inqueuep->crntPtr = 0;
        }
        ptr = &inqueuep->msgPtr[(inqueuep->crntPtr)*MAX_MSG_SIZE] ;
	while (count) {	
		get_user(ptr[tempVar++], buf++);
		count--;
		}
        inqueuep->queuesize ++;
        message = (struct msg*)ptr;
        message->enque_time = time_stamp_counter();
	//printk("\n[driver]Writing -- msg %d crnt loc:%d: %s \n", inqueuep->queuesize,inqueuep->crntPtr, inqueuep->msgPtr[inqueuep->crntPtr]);
	//printk("\n[driver]Message: id:%d sid:%d did:%d msgbuff:%s",message->mid,message->sid,message->did,message->msg_buffer);
        up(&inqueuep->sema);
	return 0;
}
/*
 * Read to squeue driver
 */
ssize_t inqueue_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	int bytes_read = 0,i,tmp;
	struct inqueue *inqueuep = file->private_data;
        char *ptr;
        struct msg *message;
	
         /*take the sema so that other threads may not modify inqueue*/
         down(&inqueuep->sema);
         /*
	 * If we're at the end of the message, 
	 * return 0 signifying end of file 
	 */
        //printk("\n [driver] read request for queue:%s\n",inqueuep->name);
	if (inqueuep->queuesize == 0)
        {
//	  printk("\n[driver] inqueue is Empty \n");	
          up(&inqueuep->sema);
          return -1;
        }

	/* 
	 * Actually put the data into the buffer
         * Always read the first msg as it is FIFO queue 
	 */
        ptr = &inqueuep->msgPtr[0];
        message = (struct msg *)ptr;
        message->accum_time += (time_stamp_counter() - message->enque_time);
        message->enque_time = time_stamp_counter();
        count = sizeof(struct msg);
	while (count ) {

		put_user(ptr[bytes_read], buf++);
		count--;
		bytes_read++;
	}
	//printk("\n[driver]Reading Message: id:%d sid:%d did:%d msgbuff:%s",message->mid,message->sid,message->did,message->msg_buffer);
        /* now re shuffle all the msg queue by removing first msg*/
        for (i = 0;i <= inqueuep->crntPtr; i++)
        {
          for(tmp = 0;tmp < MAX_MSG_SIZE;tmp++)
          {
            inqueuep->msgPtr[(i*MAX_MSG_SIZE)+tmp] = inqueuep->msgPtr[((i+1)*MAX_MSG_SIZE)+tmp];
          }
        }
        inqueuep->crntPtr --;
        inqueuep->queuesize --;
	/* free the msg buffer as we read the msg*/
        //kfree(ptr);
        /* 
	 * Most read functions return the number of bytes put into the buffer
	 */
        up(&inqueuep->sema);
	return bytes_read;

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations inqueue_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= inqueue_driver_open,        /* Open method */
    .release	        = inqueue_driver_release,     /* Release method */
    .write		= inqueue_driver_write,       /* Write method */
    .read		= inqueue_driver_read,        /* Read method */
};

/*
 * Driver Initialization
 */
int generic_driver_init(char *devname,int devno)
{
	int ret,major_no;
        struct inqueue *inqueuep;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&major_no, 0, 1, devname) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}
        inqueue_dev_number[devno] = major_no;
	/* Populate sysfs entries */
	inqueue_dev_class[devno] = class_create(THIS_MODULE, devname);

	/* Allocate memory for the per-device structure */
	inqueuep = kmalloc(sizeof(struct inqueue), GFP_KERNEL);
		
	if (!inqueuep) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */
	sprintf(inqueuep->name, devname);

	/* Connect the file operations with the cdev */
	cdev_init(&inqueuep->cdev, &inqueue_fops);
	inqueuep->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&inqueuep->cdev, (inqueue_dev_number[devno]), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	inqueue_dev_device[devno] = device_create(inqueue_dev_class[devno], NULL, MKDEV(MAJOR(inqueue_dev_number[devno]), 0), NULL, devname);		
        /*Initialize the semaphore*/
   	sema_init(&inqueuep->sema,1);
	
	inqueuep->crntPtr = 0;
        inqueuep->queuesize = 0;
        inqueuep_global[devno] = inqueuep;
	printk("inqueue driver initialized %s \n",inqueuep->name);
        return 0;

}
void generic_driver_exit(int devno)
{

   struct inqueue *inqueuep;

        inqueuep = inqueuep_global[devno];
	/* Release the major number */
	unregister_chrdev_region((inqueue_dev_number[devno]), 1);

	/* Destroy device */
	device_destroy (inqueue_dev_class[devno], MKDEV(MAJOR(inqueue_dev_number[devno]), 0));
	cdev_del(&inqueuep->cdev);
	kfree(inqueuep);
	
	/* Destroy driver_class */
	class_destroy(inqueue_dev_class[devno]);

	printk("inqueue driver removed.\n");

}
int __init inqueue_driver_init(void)
{
  int ret;
   
   ret = generic_driver_init(DEVICE_NAME_1,0);
   if(ret)
   {
    printk("\n device init failed for device :%s\n",DEVICE_NAME_1);
   }
   ret = generic_driver_init(DEVICE_NAME_2,1);
   if(ret)
   {
    printk("\n device init failed for device :%s\n",DEVICE_NAME_2);
   }
   ret = generic_driver_init(DEVICE_NAME_3,2);
   if(ret)
   {
    printk("\n device init failed for device :%s\n",DEVICE_NAME_3);
   }
   ret = generic_driver_init(DEVICE_NAME_4,3);
   if(ret)
   {
    printk("\n device init failed for device :%s\n",DEVICE_NAME_4);
   }
   return ret;
}
/* Driver Exit */
void __exit inqueue_driver_exit(void)
{
  generic_driver_exit(0);
  generic_driver_exit(1);
  generic_driver_exit(2);
  generic_driver_exit(3);
}

module_init(inqueue_driver_init);
module_exit(inqueue_driver_exit);
MODULE_LICENSE("GPL v2");
