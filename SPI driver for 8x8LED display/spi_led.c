/*
 * =====================================================================================
 *
 *       Filename:  spi_led.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  10/30/2014 02:20:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h> 
#include <linux/cdev.h>
#include <linux/delay.h>

#define DEVICE_NAME              "spiled"
#define DELAY                    0
#define SPEED                    500000
#define BITS                     8
#define LEN                      2
#define SPI_LED_TX_PATTERN       1
#define SPI_LED_INTIALISE        3
#define SPI_CS_MUX               42    
#define SPI_MOSI_MUX             43    
#define SPI_CLK_MUX              55    
#define SPI_MODE_MASK           (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                                 | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                                 | SPI_NO_CS | SPI_READY )
#define SPIDEV_MAJOR 153 

struct display
{
	unsigned int pattern_id[10];
	unsigned int delay_ms[10];
	unsigned int pattern_count;
};
struct pattern {
	uint8_t data[8];
};
struct spiled_data {
	struct cdev cdev;
        spinlock_t              spi_lock;
        struct spi_device       *spi;
        u8                      *buf;
        struct pattern          buffer[10];
	//struct display          disp_queue;
	struct task_struct      *disp_task;
	unsigned int             dispoff;
};
struct disp_request
{
	unsigned int pattern_id;
	unsigned int delay_ms;
};
struct spiled_data      *spiled;
struct class *spiled_class;               /* Tie with the device model */
static dev_t spiled_major;               /* Allotted device number */
uint8_t intensity[] = {
        0x0A,0x01,
};//intensity = 1 
uint8_t scanlimit[] = {
	0x0B,0x07,
};//scanlimit = 7 for all digits
uint8_t operation[] = {
	0x0C,0x01,
};//shutdown = 1 normal operation
uint8_t tx1[] = {
        0x01,0x19,0x02,0xFB,0x03,0xEC,0x04,0x08,0x05,0x08,0x06,0x0F,0x07,0x09,0x08,0x10
        };/* Digit 1  */


/*-------------------------------------------------------------------------*/
 
/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void spiled_complete(void *arg)
{
        complete(arg);
}
static ssize_t
spiled_sync(struct spiled_data *spiled, struct spi_message *message)
{
        DECLARE_COMPLETION_ONSTACK(done);
        int status;

        message->complete = spiled_complete;
        message->context = &done;

        if (spiled->spi == NULL)
                status = -ESHUTDOWN;
        else
                status = spi_async(spiled->spi, message);

        if (status == 0) {
                wait_for_completion(&done);
                status = message->status;
                if (status == 0)
                        status = message->actual_length;
        }
        return status;
}

/* Write-only message with current device setup */

static int spiled_message_send(struct spiled_data *spiled,unsigned int len)
{
        struct spi_transfer     k_tmp = {
					.tx_buf = spiled->buf,
					.len    = len,
};
        struct spi_message      msg;
        int                     status = -EFAULT;

        //printk("\n SPI MSG SEND len:%d:buf:%p\n",len,spiled->buf);

        spi_message_init(&msg);
        spi_message_add_tail(&k_tmp, &msg);
        status = spiled_sync(spiled, &msg);
        return status;
}
int display_off(struct spiled_data *spiled)
{
	int k,retval =0;
	for(k = 1; k <= 8; k++)
	{
	  *(spiled->buf+0) = k;
	  *(spiled->buf+1) = 0;
	  retval = spiled_message_send(spiled, 2);
	  if (retval < 0)
	     printk("\n failed to send message \n");
	}
	return retval;
}
#if 0
int display_task(void *data) 
{
	struct spiled_data      *spiled = data;
	struct display          *disp_queue;
	unsigned int     id =0,delay =0,count =0;
	int i,k,retval =0;

	disp_queue = &spiled->disp_queue;
	printk("Display task Running \n");
	while(!kthread_should_stop())
	{
          spin_lock_irq(&spiled->spi_lock);
	  count = disp_queue->pattern_count;
          spin_unlock_irq(&spiled->spi_lock);
	  if(disp_queue->pattern_count == 0)
	  {
	     retval = display_off(spiled);
	     if (retval < 0)
	       printk("\n failed to send message \n");
	       printk("Display task Exitted \n");
	       break;
	  }
	  else
	  {
       	    for(i = 0;i < disp_queue->pattern_count; i++)
	    {
               spin_lock_irq(&spiled->spi_lock);
	       id = disp_queue->pattern_id[i];
	       delay = disp_queue->delay_ms[i];
               spin_unlock_irq(&spiled->spi_lock);
	      // printk("\n The pattern id:%d,delay:%d \n",tmpbuf[i].pattern_id,tmpbuf[i].delay_ms);
	       for(k = 1; k <= 8; k++)
	       {
  		  *(spiled->buf+0) = k;
	          //printk("\n The pattern [k]:%x\n",spiled->buffer[tmpbuf[i].pattern_id].data[k-1]);
		  *(spiled->buf+1) = spiled->buffer[id].data[k-1];
		  retval = spiled_message_send(spiled, 2);
		  if (retval < 0)
		     printk("\n failed to send message \n");
	       }  
	       mdelay(delay);
	    }
	  }
	}
	spiled->disp_task = NULL;
	return 0;
}
#endif
static ssize_t
spiled_write(struct file *filp, const char __user *buf,
                size_t count, loff_t *f_pos)
{
        struct spiled_data      *spiled;
        ssize_t                 retval = 0;
        unsigned long           missing;
	unsigned int            pattern_count;
	struct disp_request         tmpbuf[10];
	//struct display		*queue;
	unsigned int     id =0,delay =0;
	int i,k;

        spiled = filp->private_data;
	//queue = &spiled->disp_queue;
	if(count > sizeof(tmpbuf))
	{
          count = sizeof(tmpbuf);
	}
	if(count == 0)
	{
	  pattern_count =0;
	  //queue->pattern_count = 0;
	  retval = display_off(spiled);
	  if (retval < 0)
	     printk("\n failed to send message \n");
	}
        else
	{
	  missing = copy_from_user(tmpbuf,buf,count) ? -EFAULT : missing;
          if (IS_ERR(tmpbuf))
              return PTR_ERR(tmpbuf);
          pattern_count = count / (sizeof(struct disp_request));
	  //queue->pattern_count = pattern_count;
	}
	for(i = 0;i < pattern_count; i++)
	{
	  // printk("\n The pattern id:%d,delay:%d \n",tmpbuf[i].pattern_id,tmpbuf[i].delay_ms);
	   id = tmpbuf[i].pattern_id;
	   delay = tmpbuf[i].delay_ms;
	   for(k = 1; k <= 8; k++)
	   {
  	      *(spiled->buf+0) = k;
	      //printk("\n The pattern [k]:%x\n",spiled->buffer[tmpbuf[i].pattern_id].data[k-1]);
	      *(spiled->buf+1) = spiled->buffer[id].data[k-1];
	      retval = spiled_message_send(spiled, 2);
	      if (retval < 0)
	         printk("\n failed to send message \n");
	    }  
	    mdelay(delay);
	}
        return retval;
}
static long
spiled_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        int                     retval = 0;
        struct spiled_data      *spiled;
	struct spi_device       *spi;
        u32                     save_1;
        u32                     save_2;
        u32                     save_3;


        spiled = filp->private_data;
	spi = spi_dev_get(spiled->spi);
        switch (cmd) {
        /* read requests */
        case SPI_LED_TX_PATTERN:
           retval = copy_from_user(spiled->buffer, (void __user *)arg, sizeof(spiled->buffer));
	   //printk("The spiled buffer is %x,%x",spiled->buffer[0].data[0],spiled->buffer[0].data[1]);
           break;
	case SPI_LED_INTIALISE:
		save_1 = spi->mode;
		save_2 = spi->bits_per_word;
		save_3 = spi->max_speed_hz;
		spi->mode = (u16)0;
		spi->bits_per_word = BITS;
		spi->max_speed_hz = SPEED;
		retval = spi_setup(spi);
		if (retval < 0)
		{
			spi->mode          = save_1;
			spi->bits_per_word = save_2;
			spi->max_speed_hz  = save_3;
		}
		
		memcpy(spiled->buf,intensity,sizeof(intensity));
		retval = spiled_message_send(spiled,sizeof(intensity));
		if (retval < 0)
		   printk("\n failed to set intensity \n");
		memset(spiled->buf,0,sizeof(intensity));
		
		memcpy(spiled->buf,scanlimit,sizeof(scanlimit));
		retval = spiled_message_send(spiled,sizeof(scanlimit));
		if (retval < 0)
		   printk("\n failed to set scanlimit \n");
		memset(spiled->buf,0,sizeof(intensity));
		
		memcpy(spiled->buf,operation,sizeof(operation));
		retval = spiled_message_send(spiled,sizeof(operation));
		if (retval < 0)
		   printk("\n failed to set opeartion to normal \n");
	        //memset(&(spiled->disp_queue),0,sizeof(struct display));
	        /* Create the display task so that it keeps the display 
                 * according to the display queue */
	        //spiled->disp_task = kthread_run(display_task, (void*)spiled, "Display _thread");
		//if (IS_ERR(spiled->disp_task)) 
		//{
	          //printk("display task creation failed\n"); 
                  //retval =  PTR_ERR(spiled->disp_task);
        	//}
		break;

	default:
	   break;
        }
	spi_dev_put(spi);
        return retval;
}
static int spiled_open(struct inode *inode, struct file *filp)
{
	//spiled = container_of(inode->i_cdev, struct spiled_data, cdev);
        filp->private_data = spiled;

	spiled->buf = kmalloc(256,GFP_KERNEL);
	if (spiled->buf == NULL)
                return -ENOMEM;
        return 0;
}

static int spiled_release(struct inode *inode, struct file *filp)
{
        struct spiled_data      *spiled;
        int                     status = 0;

        spiled = filp->private_data;
        filp->private_data = NULL;
	kfree(spiled->buf);
        return status;
}

static const struct file_operations spiled_fops = {
        .owner =        THIS_MODULE,
        .write =        spiled_write,
        .unlocked_ioctl = spiled_ioctl,
        .open =         spiled_open,
        .release =      spiled_release,
};
/*-------------------------------------------------------------------------*/

static int spiled_probe(struct spi_device *spi)
{
        /* Initialize the driver data */
        spiled->spi = spi;


        /* If we can allocate a minor number, hook up this device.
         * Reusing minors is fine so long as udev or mdev is working.
         */
        //device_create(spiled_class, &spi->dev, MKDEV(MAJOR(spiled_major), 0),
        device_create(spiled_class, &spi->dev, MKDEV(SPIDEV_MAJOR, 0),
                                    spiled, DEVICE_NAME);
        spi_set_drvdata(spi, spiled);
        return 0;
}

static int spiled_remove(struct spi_device *spi)
{
        struct spiled_data      *spiled = spi_get_drvdata(spi);

        /* make sure ops on existing fds can abort cleanly */
        //spin_lock_irq(&spiled->spi_lock);
        spiled->spi = NULL;
        //spin_unlock_irq(&spiled->spi_lock);
        //device_destroy(spiled_class, MKDEV(MAJOR(spiled_major), 0));
        device_destroy(spiled_class, MKDEV(SPIDEV_MAJOR, 0));
        return 0;
}
static const struct of_device_id spiled_dt_ids[] = {
        { .compatible = "rohm,dh2228fv" },
        {},
};

MODULE_DEVICE_TABLE(of, spiled_dt_ids);

static struct spi_driver spiled_spi_driver = {
        .driver = {
                .name =         "spidev",
                .owner =        THIS_MODULE,
                .of_match_table = of_match_ptr(spiled_dt_ids),
        },
        .probe =        spiled_probe,
        .remove =       spiled_remove,

        /* NOTE:  suspend/resume methods are not necessary here.
         * We don't do anything except pass the requests to/from
         * the underlying controller.  The refrigerator handles
         * most issues; the controller driver handles the rest.
         */
};

/*-------------------------------------------------------------------------*/

static int __init spiled_init(void)
{
	int status;

	/* Request dynamic allocation of a device major number */
        if (register_chrdev(SPIDEV_MAJOR, "spiled", &spiled_fops) < 0)
	//if (alloc_chrdev_region(&spiled_major, 0, 1, DEVICE_NAME) < 0) 
        {
	   printk(KERN_DEBUG "Can't register device\n"); return -1;
	}
        spiled_class = class_create(THIS_MODULE, DEVICE_NAME);
        if (IS_ERR(spiled_class)) {
                unregister_chrdev_region(spiled_major, 1);
	   printk("class create faile\n"); 
                return PTR_ERR(spiled_class);
        }
        /* Allocate driver data */
        spiled = kmalloc(sizeof(struct spiled_data), GFP_KERNEL);
        if (!spiled)
                return -ENOMEM;

	///* Connect the file operations with the cdev */
	//cdev_init(&spiled->cdev, &spiled_fops);
	//spiled->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	//   printk("cdevadd \n"); 
	//status = cdev_add(&spiled->cdev, (spiled_major), 1);
	//status = cdev_add(&spiled->cdev, SPIDEV_MAJOR, 1);

	//if (status) 
	//{
	//   printk("Bad cdev\n"); 
	//   return status;
	//}
        status = spi_register_driver(&spiled_spi_driver);
        if (status < 0) 
	{
            class_destroy(spiled_class);
            unregister_chrdev_region(spiled_major, 1); 		
	   printk("reg failed\n"); 
	   return status;
        }
        /* set the mux pin so that SPI bus is connected to pins */
        spin_lock_init(&spiled->spi_lock);
        gpio_request(SPI_CS_MUX, "cs_mux");
        gpio_direction_output(SPI_CS_MUX,0); 
        mdelay(10);
        gpio_set_value_cansleep(SPI_CS_MUX,0);
        mdelay(10);
        gpio_request(SPI_MOSI_MUX, "mosi_mux");
        gpio_direction_output(SPI_MOSI_MUX,0); 
        mdelay(10);
        gpio_set_value_cansleep(SPI_MOSI_MUX,0);
        mdelay(10);
        gpio_request(SPI_CLK_MUX, "clk_mux");
        gpio_direction_output(SPI_CLK_MUX,0); 
        mdelay(10);
        gpio_set_value_cansleep(SPI_CLK_MUX,0);
        mdelay(10);
	printk("display driver initialized\n"); 
        return status;
}
static void __exit spiled_exit(void)
{
	if(spiled->disp_task != NULL)
	{
	  kthread_stop(spiled->disp_task);
	}
        spi_unregister_driver(&spiled_spi_driver);
        class_destroy(spiled_class);
        //unregister_chrdev_region(spiled_major, 1);
        unregister_chrdev(SPIDEV_MAJOR, "spiled");
	//cdev_del(&spiled->cdev);
	printk("display driver Removed\n"); 
        kfree(spiled);
	gpio_free(SPI_CS_MUX);
        gpio_free(SPI_MOSI_MUX);
        gpio_free(SPI_CLK_MUX);

}
module_init(spiled_init);
module_exit(spiled_exit);
MODULE_LICENSE("GPL v2");

