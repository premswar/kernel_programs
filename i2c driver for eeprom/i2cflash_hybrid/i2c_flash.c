/*
 * =====================================================================================
 *
 *       Filename:  i2c_flash.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  10/05/2014 08:01:25 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *
 * =====================================================================================
 */

  #include <linux/kernel.h>
  #include <linux/module.h>
  #include <linux/device.h>
  #include <linux/sysfs.h>
  #include <linux/notifier.h>
  #include <linux/fs.h>
  #include <linux/slab.h>
  #include <linux/init.h>
  #include <linux/list.h>
  #include <linux/i2c.h>
  #include <linux/i2c-dev.h>
  #include <linux/jiffies.h>
  #include <linux/uaccess.h>
  #include <linux/gpio.h> 
  #include <linux/cdev.h>
  #include <linux/delay.h>
  #include <linux/errno.h>

#define DEBUG_I2C          0
#define NON_BLOCKING       0
#define ADAPTER_NO         0
#define EEPROM_ADDR        0x54
#define I2C_MUX            29
#define LED                26
#define DEVICE_NAME        "i2c_flash"           /* device name to be created and registered*/
#define	MAX_PAGES          512			 /* Maximum numbeer of pages in EEPROM */
#define	MAX_PAGE_SIZE      64			 /* Maximum numbeer of pages in EEPROM */
#define	FLASHGETS          3                     /*ioctl command to get status if eeprom busy or not   */
#define	FLASHGETP	   4	                 /*ioctl command to get status of current page pointer */
#define	FLASHSETP          5	                 /*ioctl command to set status of current page pointer */
#define	FLASHERASE	   6	                 /*ioctl command to erase all memory in eeprom         */

/* #####   DATA TYPES  -  LOCAL TO THIS SOURCE FILE   ############################### */
/* per device structure */
struct i2c_flash_dev {
	struct cdev cdev;                        /* The cdev structure */
	struct i2c_client *client;
        unsigned short crnt_page_pointer;
} *i2c_flash_devp;

typedef struct {
	struct work_struct my_work;
	struct i2c_flash_dev *flash_devp;
	char *local_buf;
	unsigned int  count;
        bool data_ready;
} i2c_work_t;
/* #####   VARIABLES  -  LOCAL TO THIS SOURCE FILE   ################################ */

static dev_t i2c_flash_dev_number;               /* Allotted device number */
struct class *i2c_flash_dev_class;               /* Tie with the device model */
static struct device *i2c_flash_device;
#if NON_BLOCKING
static struct workqueue_struct *i2c_wq;          /* work queue */
#endif
i2c_work_t *work_read, *work_write;
/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ##################### */

static void i2c_wq_read( struct work_struct *work) 
{
	
	i2c_work_t *i2c_work_read = (i2c_work_t *)work;
        char *user_copy = i2c_work_read->local_buf;
        int ret =0,i;
        unsigned int count = i2c_work_read->count;
	struct i2c_flash_dev *flash_devp = i2c_work_read->flash_devp;
        struct i2c_client *client = flash_devp->client;
	char *tmp;
#if  DEBUG_I2C
	int j;
#endif     /* -----  DEBUG_I2C  ----- */

	gpio_set_value_cansleep(LED,1);

	for ( i = 0;i < count ;i++ )
        {
           tmp =  (user_copy +(i*MAX_PAGE_SIZE));          
           ret = i2c_master_recv(client, tmp, MAX_PAGE_SIZE);
           if (ret >= 0)
           {
           #if DEBUG_I2C
              printk("Rcvd msg:");
              for (j = 0; j < MAX_PAGE_SIZE; j++)
              {
                 printk("%x  ",tmp[j]);
              }
              printk("\n");
           #endif
              flash_devp->crnt_page_pointer++;
           }
           mdelay(10);
        }
        i2c_work_read->data_ready = true;

        gpio_set_value_cansleep(LED,0);
	return;
}
static ssize_t i2cdev_read(struct file *file, char __user *buf, size_t count,
                           loff_t *offset)
{
	char *user_copy;
	int ret =0,res;
	if (count > MAX_PAGES)
            count = MAX_PAGES;
 
#if NON_BLOCKING
	if((work_read))
        {
           if((work_read->data_ready == false))
           {
              return -EBUSY;
           }
           else
           {
              user_copy = work_read->local_buf;
              res = copy_to_user(buf, user_copy,(count*MAX_PAGE_SIZE));
              if(res < 0 )
              {
                 printk("Err copy to user:%d \n",res);
              }
              work_read->data_ready = false;
              kfree(user_copy);
              kfree(work_read);
              work_read = 0;
              return 0;
           }
        }
        else
        {
#if  DEBUG_I2C
           printk("\n creating wrk queue \n");
#endif     /* -----  DEBUG_I2C  ----- */
	   if (i2c_wq) 
           {
#endif     /* ----NON_BLOCKING */
	      work_read = (i2c_work_t *)kmalloc(sizeof(i2c_work_t), GFP_KERNEL);
	      if (work_read)
 	      {		// Queue work (item 1)
#if NON_BLOCKING
	         INIT_WORK( (struct work_struct *)work_read, i2c_wq_read );
#endif     /* ----NON_BLOCKING */
	         work_read->flash_devp = file->private_data;
                 work_read->local_buf = kmalloc((MAX_PAGE_SIZE * count), GFP_KERNEL);
                 if ( work_read->local_buf == NULL)
                      return -ENOMEM;
                 work_read->count = count;
                 work_read->data_ready = false; 
#if  NON_BLOCKING
	         ret = queue_work( i2c_wq, (struct work_struct *)work_read );
                 ret =  -EAGAIN;
#else      /* -----  not NON_BLOCKING  ----- */
		 i2c_wq_read((struct work_struct *)work_read );
                 if(work_read->data_ready == true)
	         {
                    user_copy = work_read->local_buf;
                    res = copy_to_user(buf, user_copy,(count*MAX_PAGE_SIZE));
                    if(res < 0 )
                    {
                       printk("Err copy to user:%d \n",res);
                    }
                    work_read->data_ready = false;
                    kfree(user_copy);
                    kfree(work_read);
                    work_read = 0;
                    return 0;
                 }
                 else
                 {
                    return -EBUSY;
                 }
#endif     /* -----  not NON_BLOCKING  ----- */
	      }
#if NON_BLOCKING
           }
#endif     /* ----NON_BLOCKING */
           return ret;
#if NON_BLOCKING
        }
#endif     /* ----NON_BLOCKING */
}
 
static void i2c_wq_write( struct work_struct *work) 
{
 	i2c_work_t *i2c_work_write = (i2c_work_t *)work;
        int ret = 0,i;
        char *tmp;
        char *local_buf = i2c_work_write->local_buf;
        unsigned int count = i2c_work_write->count;
        struct i2c_flash_dev *flash_devp = i2c_work_write->flash_devp;
        struct i2c_client *client = flash_devp->client;
#if  DEBUG_I2C
	int j;
#endif     /* -----  DEBUG_I2C  ----- */

 
        gpio_set_value_cansleep(LED,1);
       
        for(i = 0; i < count; i++)
        { 
         #if DEBUG_I2C 
           printk("\n calling master send for client:0x%x \n",client->addr); 
         #endif
           tmp = (local_buf+(i*(MAX_PAGE_SIZE+2)));
           ret = i2c_master_send(client,tmp, (MAX_PAGE_SIZE+2));
         #if DEBUG_I2C 
           printk("sent msg:");
           for (j = 0; j < MAX_PAGE_SIZE+2; j++)
           {
             printk("%2x  ",tmp[j]);
           }
           printk("\n");
         #endif
           mdelay(10);
        }
       
        kfree(local_buf);
	kfree( (void *)work_write );
        work_write =0;

        gpio_set_value_cansleep(LED,0);
	return;
}
static ssize_t i2cdev_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *offset)
{
        int ret = 0,i;
        char *tmp,*local_buf;
        unsigned short hilow =0;
        struct i2c_flash_dev *flash_devp = file->private_data;

#if NON_BLOCKING
	if(work_write)
        {
           return -EBUSY;
        }
#endif     /* ----NON_BLOCKING */
 
        if (count > MAX_PAGES)
                count = MAX_PAGES;
 
        tmp = kmalloc(MAX_PAGE_SIZE,GFP_KERNEL);
        local_buf = kmalloc(((MAX_PAGE_SIZE+2)*count),GFP_KERNEL);
        memset(tmp,0,MAX_PAGE_SIZE);
        memset(local_buf,0,((MAX_PAGE_SIZE+2)*count)); 

        for(i = 0; i < count; i++)
        {          
           hilow = ((flash_devp->crnt_page_pointer)*MAX_PAGE_SIZE);
           hilow = hilow & 0x7fc0; // make sure its 15 bits
           hilow = htons(hilow);
           memcpy((local_buf+(i * (MAX_PAGE_SIZE+2))),&hilow,2);
	   ret = copy_from_user(tmp,(buf + (i * MAX_PAGE_SIZE)),MAX_PAGE_SIZE ) ? -EFAULT : ret;
           if (IS_ERR(tmp))
              return PTR_ERR(tmp);
           memcpy((local_buf+(i * (MAX_PAGE_SIZE+2))+2),tmp ,MAX_PAGE_SIZE);
           flash_devp->crnt_page_pointer++;
           if(flash_devp->crnt_page_pointer == MAX_PAGES)
           {
              flash_devp->crnt_page_pointer = 0;
           }
        }

#if NON_BLOCKING
	if (i2c_wq) 
        {
#endif     /* ----NON_BLOCKING */
         #if DEBUG_I2C 
           printk("\n Entered to create queue \n");
         #endif
	   work_write = (i2c_work_t *)kmalloc(sizeof(i2c_work_t), GFP_KERNEL);
	   if (work_write)
 	   {
         #if DEBUG_I2C 
              printk("\n queue created :%p\n",work_write);
         #endif
#if NON_BLOCKING
	      INIT_WORK( (struct work_struct *)work_write, i2c_wq_write );
#endif     /* ----NON_BLOCKING */
	      work_write->flash_devp = file->private_data;
              work_write->local_buf = local_buf;
              work_write->count = count;
#if  NON_BLOCKING
	      ret = queue_work( i2c_wq, (struct work_struct *)work_write );
	      
#else      /* -----  not NON_BLOCKING  ----- */
	      i2c_wq_write((struct work_struct *)work_write);
              ret =0;
#endif     /* -----  not NON_BLOCKING  ----- */
         #if DEBUG_I2C 
              printk("\n posted to queue \n");
              mdelay(20);
         #endif
	   }
#if NON_BLOCKING
        }
#endif     /* ----NON_BLOCKING */
        return ret;
}

static noinline int i2c_flash_ioctl_setp(struct i2c_client *client,
                                         unsigned long arg)
{
        int ret;
        char tmp[2];
        unsigned short hilow;
          
         
#if  NON_BLOCKING
        if((work_read)||(work_write))
        {
           return -EBUSY;/* if any of the work  queues exist they are processing so busy */
        }
#endif     /* ----NON_BLOCKING */
        hilow = arg*MAX_PAGE_SIZE;
        hilow = hilow & 0x7fc0; // make sure its 15 bits
        hilow = htons(hilow);
        memcpy(tmp,&hilow,2);
        ret = i2c_master_send(client, tmp,2);
        return ret;
}
static noinline int i2c_flash_ioctl_erase(struct i2c_client *client)
{
        int ret,i;
        char *tmp;
        unsigned short hilow;
          
        tmp = kmalloc((MAX_PAGE_SIZE+2),GFP_KERNEL);
        gpio_set_value_cansleep(LED,1);

#if  NON_BLOCKING
        if((work_read)||(work_write))
        {
           return -EBUSY;/* if any of the work  queues exist they are processing so busy */
        }
#endif     /* ----NON_BLOCKING */
        for (i = 0; i < MAX_PAGES; i++)
        {
           hilow = (i*MAX_PAGE_SIZE) & 0x7ff; // make sure its 15 bits
           hilow = htons(hilow);
           memcpy(tmp,&hilow,2);
           memset((tmp+2),0xff,MAX_PAGE_SIZE); /* writing all 1's to erase*/
           ret = i2c_master_send(client, tmp, (MAX_PAGE_SIZE+2));
           mdelay(10);
        }
        kfree(tmp);
        gpio_set_value_cansleep(LED,0);
        return ret;
}
static long i2cdev_ioctl(struct file *file, unsigned int request, unsigned long arg)
{
        struct i2c_flash_dev *flash_devp = file->private_data;
        struct i2c_client *client = flash_devp->client;
        int ret;
        unsigned long page;

         #if DEBUG_I2C 
        printk("\n ioctl func req:%d\n",request); 
         #endif
        switch (request) 
        {
           case FLASHGETS:
#if  NON_BLOCKING
              if((work_read)||(work_write))
              {
                return -EBUSY;/* if any of the work  queues exist they are processing so busy */
              }
#endif     /* ----NON_BLOCKING */
                 /* set the page pointer to same loction to know eeprom is busy or not  */
              ret = i2c_flash_ioctl_setp(client,flash_devp->crnt_page_pointer);
              if(ret <= 0)
                 return -EBUSY;

                 return ret;
           case FLASHGETP:
                 
              page = (unsigned long)flash_devp->crnt_page_pointer;
         #if DEBUG_I2C 
              printk("\n crnt page ptr:%lu \n",page);
         #endif
	      ret = copy_to_user((void *)arg, &page, sizeof (unsigned long));      
              mdelay(10);
              return ret;
           case FLASHSETP:
              ret = i2c_flash_ioctl_setp(client,arg);
              flash_devp->crnt_page_pointer = arg;
              return ret;
           case FLASHERASE:
              ret = i2c_flash_ioctl_erase(client);
              flash_devp->crnt_page_pointer = 0;
              return ret;
           default:
         #if DEBUG_I2C 
              printk("\n default case inioctl:%d",request);
              mdelay(10);
         #endif
              return -ENOTTY;
        }
        return 0;
}

static int i2cdev_open(struct inode *inode, struct file *file)
{
        struct i2c_client *client;
        struct i2c_adapter *adap;
                            
        adap = i2c_get_adapter(ADAPTER_NO);//get the first adapter in bus0
        if (!adap)
                return -ENODEV;
 
        /* This creates an anonymous i2c_client,
         */
        client = kzalloc(sizeof(*client), GFP_KERNEL);
        if (!client) 
        {
           i2c_put_adapter(adap);
           return -ENOMEM;
        }
        snprintf(client->name, I2C_NAME_SIZE, "i2c_flash");
 
        client->adapter = adap;
        client->addr = EEPROM_ADDR;
        i2c_flash_devp->client = client;
	i2c_flash_devp->crnt_page_pointer = 0;
        file->private_data = i2c_flash_devp;
        gpio_export(LED, true);
        gpio_direction_output(LED,0); 
        return 0;
}
 
static int i2cdev_release(struct inode *inode, struct file *file)
{
        struct i2c_flash_dev *flash_devp = file->private_data;
        struct i2c_client *client = flash_devp->client;

        i2c_put_adapter(client->adapter);
        kfree(client);
        flash_devp->crnt_page_pointer = 0;
        file->private_data = NULL; 	
        return 0;
}

static const struct file_operations i2c_flash_fops = {
        .owner          = THIS_MODULE,
        .llseek         = no_llseek,
        .read           = i2cdev_read,
        .write          = i2cdev_write,
        .unlocked_ioctl = i2cdev_ioctl,
        .open           = i2cdev_open,
        .release        = i2cdev_release,
}; 

int __init i2c_flash_driver_init(void)
{
	int ret;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&i2c_flash_dev_number, 0, 1, DEVICE_NAME) < 0) 
        {
	   printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	i2c_flash_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	i2c_flash_devp = kmalloc(sizeof(struct i2c_flash_dev), GFP_KERNEL);
		
	if (!i2c_flash_devp) 
        {
	   printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	
	/* Connect the file operations with the cdev */
	cdev_init(&i2c_flash_devp->cdev, &i2c_flash_fops);
	i2c_flash_devp->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&i2c_flash_devp->cdev, (i2c_flash_dev_number), 1);

	if (ret) 
        {
	   printk("Bad cdev\n");
	   return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	i2c_flash_device = device_create(i2c_flash_dev_class, NULL, MKDEV(MAJOR(i2c_flash_dev_number), 0), NULL, DEVICE_NAME);		

#if  NON_BLOCKING
        i2c_wq = create_workqueue("i2c_queue");// create work queue
#endif     /* ----NON_BLOCKING */
        /* set the mux pin so that i2c has the SDA pin */
        gpio_export(I2C_MUX, true);
        gpio_direction_output(I2C_MUX,0); 
        gpio_set_value_cansleep(I2C_MUX,0);
   
	printk("I2C_FLASH driver initialized.\n");
	return 0;
}
/* Driver Exit */
void __exit i2c_flash_driver_exit(void)
{
	/* Release the major number */
	unregister_chrdev_region((i2c_flash_dev_number), 1);

	/* Destroy device */
	device_destroy (i2c_flash_dev_class, MKDEV(MAJOR(i2c_flash_dev_number), 0));
	cdev_del(&i2c_flash_devp->cdev);
	kfree(i2c_flash_devp);
	
	/* Destroy driver_class */
	class_destroy(i2c_flash_dev_class);
#if  NON_BLOCKING
        /* Destroy work queue */
        destroy_workqueue(i2c_wq);
#endif     /* ----NON_BLOCKING */
	printk("I2C_FLASH driver removed.\n");
}

module_init(i2c_flash_driver_init);
module_exit(i2c_flash_driver_exit);
MODULE_LICENSE("GPL v2");
