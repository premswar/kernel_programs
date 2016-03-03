/*
 * =====================================================================================
 *
 *       Filename:  sensor.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  11/04/2014 12:37:38 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *
 * =====================================================================================
 */

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
#include <linux/gpio.h>
#include<linux/init.h>
#include<linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define DRIVER_NAME                  "sensor"
#define TRIGGER                       14
#define ECHO                          15
#define TRIGGER_MUX                   31
#define ECHO_MUX                      30
#define DIVFACTOR		      23529
#define RISING                        1
#define FALLING                       2
#define TRUE                          1
#define FALSE                         0

//typedef unsigned long long u64;
//typedef unsigned long      u32;
   
static inline u64 rdtsc(void)
{
	if (sizeof(long) == sizeof(u64)) {
             u32 lo, hi;
             asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
            return ((u64)(hi) << 32) | lo;
        }
        else {
              u64 tsc;
              asm volatile("rdtsc" : "=A" (tsc));
              return tsc;
        }
 } 
/**
 * sensor structure associated with each device
 */
struct sensor_data{
	struct cdev cdev;
	unsigned long distance;
	unsigned int busy;
	unsigned int direction;
	unsigned long long timestamp;
    	struct semaphore semLock;  
};
static dev_t sensor_dev_number;      /* Allotted device number */
struct class *sensor_dev_class;          /* Tie with the device model */
unsigned int sensor_irq;
struct sensor_data *sensor1;
/*
* Open sensor driver
*/
int sensor_driver_open(struct inode *inode, struct file *file)
{
	struct sensor_data *sensor1;
	sensor1 = container_of(inode->i_cdev, struct sensor_data, cdev);
	file->private_data = sensor1;
	printk("\n opening sensor driver\n" );
	return 0;
}
/*
 * Release sensor driver
 */
int sensor_driver_release(struct inode *inode, struct file *file)
{
	struct sensor_data *sensor1;
	
	sensor1 = file->private_data;
	printk(KERN_DEBUG "\n Sensor is released Released \n");
	return 0;
}
irqreturn_t echo_handle(int irq,void *dev_id)
{
	unsigned long  timediff;
	int retval = IRQ_NONE;
	struct sensor_data *sensor1 = dev_id;
	if(sensor1->direction == RISING)
	{
	  sensor1->timestamp = rdtsc();
	  sensor1->direction = FALLING;
	  irq_set_irq_type(sensor_irq, IRQ_TYPE_EDGE_FALLING);
	  retval = IRQ_HANDLED;
	}
	else
	{
          timediff = (unsigned long)(rdtsc() - (sensor1->timestamp));
	  sensor1->distance = timediff/DIVFACTOR;
	  sensor1->direction = RISING;
	  irq_set_irq_type(sensor_irq, IRQ_TYPE_EDGE_RISING);
	  sensor1->busy = FALSE;	
	  retval = IRQ_HANDLED;
	}
	return retval;

}
/*
 * Write to sensor driver
 */
ssize_t sensor_driver_write(struct file *file, const char __user *buf,
           size_t count, loff_t *ppos)
{
	struct sensor_data *sensor1 = file->private_data;
	
	down(&sensor1->semLock);
	gpio_set_value_cansleep(TRIGGER,1);
	udelay(10);
	gpio_set_value_cansleep(TRIGGER,0);
	up(&sensor1->semLock);
	irq_set_irq_type(sensor_irq, IRQ_TYPE_EDGE_RISING);
	sensor1->busy = TRUE;	
    return 0;
}
ssize_t sensor_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	struct sensor_data *sensor1 = file->private_data;
	if(sensor1->busy == TRUE)
	{
	  return -1;
	  printk("\n Sensor BUsy \n");
	}
	else
	{
	  return ((unsigned int)sensor1->distance);
	}
    return 0;
}
/* File operations structure. Defined in linux/fs.h */
static struct file_operations sensor_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= sensor_driver_open,        /* Open method */
    .release	        = sensor_driver_release,     /* Release method */
    .write		= sensor_driver_write,       /* Write method */
    .read		= sensor_driver_read,        /* Read method */
};
/*
 * Driver Initialization
 */
int __init sensor_driver_init(void)
{
	int ret;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&sensor_dev_number, 0, 1, DRIVER_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}
	sensor_dev_class = class_create(THIS_MODULE, DRIVER_NAME);
	sensor1 = (struct sensor_data *)(kmalloc(sizeof(struct sensor_data), GFP_KERNEL));
	memset(sensor1,0,sizeof(struct sensor_data));	
	cdev_init(&sensor1->cdev, &sensor_fops);
	sensor1->cdev.owner = THIS_MODULE;
	

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&(sensor1->cdev), MKDEV(MAJOR(sensor_dev_number),0), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}
	
	/* Send uevents to udev, so it'll create /dev nodes */	
	device_create(sensor_dev_class, NULL, MKDEV(MAJOR(sensor_dev_number), 0), NULL, DRIVER_NAME);	
	
	
    if (sensor1 == NULL){
        printk ("Error Initialising the sensors kmalloc\n"); 
        return -ENOMEM;
    }
    	
    sema_init(&sensor1->semLock, 1);
	gpio_request(ECHO_MUX, "echo_mux");
        gpio_direction_output(ECHO_MUX,0); 
        mdelay(10);
        gpio_set_value_cansleep(ECHO_MUX,0);
        mdelay(10);
        gpio_request(TRIGGER_MUX, "trig_mux");
        gpio_direction_output(TRIGGER_MUX,0); 
        mdelay(10);
        gpio_set_value_cansleep(TRIGGER_MUX,0);
        mdelay(10);
        gpio_request(TRIGGER, "trig");
        gpio_direction_output(TRIGGER,0); 
        mdelay(10);
        gpio_set_value_cansleep(TRIGGER,0);
        mdelay(10);
        gpio_request(ECHO, "echo");
        gpio_direction_input(ECHO); 
        mdelay(10);
	sensor_irq = gpio_to_irq(ECHO);
	ret = request_irq(sensor_irq,echo_handle,IRQ_TYPE_EDGE_RISING,"echoirq",sensor1);
	return 0;
}
/* Driver Exit */
void __exit sensor_driver_exit(void)
{
	
	free_irq(sensor_irq,sensor1);
	/* Release the major number */
	unregister_chrdev_region((sensor_dev_number), 1);

	device_destroy (sensor_dev_class, MKDEV(MAJOR(sensor_dev_number), 0));
	
	cdev_del(&sensor1->cdev);
	
	kfree(sensor1);
	
	/* Destroy driver_class */
	class_destroy(sensor_dev_class);
	gpio_free(ECHO_MUX);
        gpio_free(TRIGGER_MUX);
        gpio_free(TRIGGER);
        gpio_free(ECHO);

	printk(KERN_DEBUG "sensor driver removed.\n");
}
module_init(sensor_driver_init);
module_exit(sensor_driver_exit);
MODULE_LICENSE("GPL v2");

