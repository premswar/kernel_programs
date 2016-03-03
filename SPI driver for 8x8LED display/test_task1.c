/*
 * =====================================================================================
 *
 *       Filename:  spi_test_led.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  10/29/2014 11:02:33 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>  /* Semaphore */
#include <signal.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define GPIO_RISING_EDGE  0
#define GPIO_FALLING_EDGE 1
#define GPIO_DIRECTION_OUT  0
#define GPIO_DIRECTION_IN 1
#define DIVFACTOR		      23529

typedef unsigned long long u64;
typedef unsigned long      u32;
static const char *spi_device = "/dev/spidev1.0";
int terminate =0;
static uint32_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay =0;
unsigned long distance =0;
sem_t mutex_distance;
uint8_t txi[] = {
0x0A,0x01,
};//intensity = 1 
uint8_t txsl[] = {
0x0B,0x07,
};//scanlimit = 7 for all digits
uint8_t txs[] = {
0x0C,0x01,
};//shutdown = 1 normal operation
uint8_t txcb[] = {
0x09,0xFF,
};//code b mode
uint8_t tx0[] = {
0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00,0x05,0x00,0x06,0x00,0x07,0x00,0x08,0x00
};/* Digit 1  */
uint8_t tx1[] = {
0x01,0x19,0x02,0xFB,0x03,0xEC,0x04,0x08,0x05,0x08,0x06,0x0F,0x07,0x09,0x08,0x10
};/* Digit 1  */
uint8_t tx2[] = {
0x01,0x18,0x02,0xFF,0x03,0xE9,0x04,0x08,0x05,0x0B,0x06,0x0E,0x07,0x08,0x08,0x04
};/* Digit 1  */
uint8_t tx3[] = {
0x01,0x10,0x02,0x09,0x03,0x0f,0x04,0x08,0x05,0x08,0x06,0xEC,0x07,0xFB,0x08,0x19
};/* Digit 1  */
uint8_t tx4[] = {
0x01,0x04,0x02,0x08,0x03,0x0E,0x04,0x0B,0x05,0x08,0x06,0xE9,0x07,0xFF,0x08,0x18
};/* Digit 1  */
uint8_t tx5[] = {
0x01,0x10,0x02,0x27,0x03,0xF8,0x04,0x27,0x05,0x10,0x06,0x01,0x07,0x00,0x08,0x01
};/* Digit 1  */
uint8_t tx6[] = {
0x01,0x11,0x02,0x26,0x03,0xf9,0x04,0x26,0x05,0x11,0x06,0x00,0x07,0x01,0x08,0x00
};/* Digit 1  */

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static void terminatehdl (int sig, siginfo_t *siginfo, void *context)
{
//        printf ("Sending PID: %ld, UID: %ld\n ",
//                        (long)siginfo->si_pid, (long)siginfo->si_uid);
        terminate = 1;
}

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
/* update the distance*/
void set_distance(unsigned long val)
{
   sem_wait(&mutex_distance);       /* down semaphore */
   distance = val;
   sem_post(&mutex_distance);       /* up semaphore */
   return;
}
/* Get the distance*/
unsigned long get_distance()
{
   unsigned long dist;
   sem_wait(&mutex_distance);       /* down semaphore */
   dist = distance;
   sem_post(&mutex_distance);       /* up semaphore */
   return dist;
}
void move_left(int fd,struct spi_ioc_transfer *tr)
{
	int k,ret,i;
	for(k = 0; k< 2; k++)
        {
          for(i =0;i< 16;i=i+2)
    	  {
	    tr->tx_buf = (unsigned long )&tx1[i];
	    ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	    if (ret < 1)
	  	pabort("can't send spi message move left");
	  }
          usleep(100000);
          for(i =0;i< 16;i=i+2)
    	  {
	    tr->tx_buf = (unsigned long )&tx2[i];
	    ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	    if (ret < 1)
	  	pabort("can't send spi message move left 2");
	  }
          usleep(100000);
	}	
}
void move_right(int fd,struct spi_ioc_transfer *tr)
{
	int k,ret,i;
        for(i =0;i< 16;i=i+2)
    	{
	   tr->tx_buf = (unsigned long )&tx3[i];
	   ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	   if (ret < 1)
	 	pabort("can't send spi message move right");
	}
        usleep(200000);
        for(i =0;i< 16;i=i+2)
    	{
	  tr->tx_buf = (unsigned long )&tx4[i];
	  ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	  if (ret < 1)
	    pabort("can't send spi message move right 2");
	 }
         usleep(200000);
}
void display_off(int fd,struct spi_ioc_transfer *tr)
{
	int k,ret,i;
        for(i =0;i< 16;i=i+2)
    	{
	   tr->tx_buf = (unsigned long )&tx0[i];
	   ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	   if (ret < 1)
	 	pabort("can't send spi message display off");
	}
        usleep(200000);
        usleep(200000);
}
void display_nums(int fd,struct spi_ioc_transfer *tr)
{
	int ret,i,k;
	uint8_t txn[2];

	//tr->tx_buf = (unsigned long )txcb;
	//ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	//if (ret < 1)
	//	pabort("can't send spi message decode mode");

        for(i =1;i<= 8 ;i++)
    	{
	   txn[0] = i;
	   txn[1] = 0xAA;
	   for(k = 0;k <8;k++)
	   {
	     tr->tx_buf = (unsigned long )&txn;
	     ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	     if (ret < 1)
	 	pabort("can't send spi message display off");
	     usleep(100000);
	     txn[1] = txn[1]*2;
	   }
	}
        usleep(200000);
        usleep(200000);
	//tr->tx_buf = (unsigned long )txs;
	//ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	//if (ret < 1)
	//	pabort("can't send spi message  normal");
}
int init_display(int fd,struct spi_ioc_transfer *tr)
{
	int ret;
	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");
	usleep(10000);
	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	tr->tx_buf = (unsigned long )txi;
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	if (ret < 1)
		pabort("can't send spi message intensity");
	tr->tx_buf = (unsigned long )txsl;
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	if (ret < 1)
		pabort("can't send spi message scan limit");
	tr->tx_buf = (unsigned long )txs;
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), tr);
	if (ret < 1)
		pabort("can't send spi message normal");
	return ret;
}
void *display_task(void *ptr)
{
	int fd ,cnt =0;
	int ret,i,k;
	unsigned long dist =0,prevdist =0,diff;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)txi,
		.rx_buf = 0,
		.len = ARRAY_SIZE(txi),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
		.cs_change = 1,
		.pad =0,
	};
	fd = open(spi_device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	ret = init_display(fd,&tr);
	if (ret < 0)
		pabort("cannot init display");
	while((!terminate)&&(cnt < 120)){	
	  dist = get_distance();
	  printf("\ndistance is %lu \n",dist);
	  diff  = dist - prevdist;
	  if(diff<0)
	  {
	    diff = prevdist - dist;
	  }
	  if(dist < 40)
	  {
	      move_left(fd,&tr);
          }
	  else 
	  {
	      move_right(fd,&tr);
          }
	  prevdist = dist;
	  cnt++;
	}
	display_off(fd,&tr);
	display_nums(fd,&tr);
	display_off(fd,&tr);
	close(fd);
	return;
}
void initGPIO(int gpio, int direction)
{
        char buffer[256];
        int fileHandle;
        int fileMode;

  //Export GPIO
        fileHandle = open("/sys/class/gpio/export", O_WRONLY);
        if(fileHandle < 0)
        {
               puts("Error: Unable to opening /sys/class/gpio/export");
               return;
        }
        sprintf(buffer, "%d", gpio);
        write(fileHandle, buffer, strlen(buffer));
        close(fileHandle);
 
   //Direction GPIO
        sprintf(buffer, "/sys/class/gpio/gpio%d/direction", gpio);
        fileHandle = open(buffer, O_WRONLY);
        if(fileHandle < 0)
        {
               puts("Unable to open file:");
               puts(buffer);
               return;
        }
 
        if (direction == GPIO_DIRECTION_OUT)
        {
               // Set out direction
               write(fileHandle, "out", 3);
               fileMode = O_WRONLY;
        }
        else
        {
               // Set in direction
               write(fileHandle, "in", 2);
               fileMode = O_RDONLY;
        }
        close(fileHandle);
        return; 
}
void deinitGPIO(int gpio)
{
        char buf[256];
        int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	sprintf(buf, "%d", gpio);
	write(fd, buf, strlen(buf));
	close(fd);
}
void writeGPIO(int gpio, int value )
{
        char buffer[256];
        int fileHandle;
        int fileMode;

   //Open GPIO for Read / Write
        fileMode = O_WRONLY;
        sprintf(buffer, "/sys/class/gpio/gpio%d/value", gpio);
        fileHandle = open(buffer, fileMode);
        if(fileHandle < 0)
        {
               puts("Unable to open file:");
               puts(buffer);
               return;
        }
        // Set value
        if(value == 0)
	{
          write(fileHandle, "0", 1);
	}
	else
	{
	  write(fileHandle, "1", 1);
	}
	//close fd
        close(fileHandle);
	return;
}

void edgeGPIO(int fd, int edge )
{
        char buffer[256];
        // write edge
        if(edge == GPIO_RISING_EDGE)
	{
	   write(fd,"rising",6);
	}
	else
	{
	   write(fd,"falling",7);
	}
	return ;
}
int pollGPIO(int fd, int timeout )
{
	int value,rc;
	struct pollfd fdset[1];
	//fd_set readset;

	fdset[0].fd = fd;
	fdset[0].events = POLLPRI;
	rc = poll(fdset,1,-1);
	if(rc < 0)
	{
	   printf("\n poll failed\n");
	   return -1;
	}
        // read value
        if(fdset[0].revents & POLLPRI)
	{
	  //printf("GPIO interrupt happened:%d\n",rc);
          pread(fd,&value,1,0);
          if(value == '0')
	  {
	     value = 0;
	  }
	  else
	  {
	     value = 1;
	  }
	}
	//printf("Value of poll:%d\n",value);
	return value;
}
void *sensor_task(void *ptr)
{
	int fd ,cnt =0;
	int ret,i,k;
	int fd15val,fd15dir,val;
	unsigned long long timestamp1;
	unsigned long dist,timediff;
	fd15val = open("/sys/class/gpio/gpio15/value", O_RDWR);
	if (fd15val < 0)
		pabort("can't open device");

	fd15dir = open("/sys/class/gpio/gpio15/edge", O_RDWR);
	if (fd15dir < 0)
		pabort("can't open device");
	pread(fd15val,&val,1,0);
	while((!terminate)&&(cnt < 120))
	{
	  //printf("writing pulse\n");
	  writeGPIO(14,1);
	  usleep(10);
	  writeGPIO(14,0);
	  edgeGPIO(fd15dir,GPIO_RISING_EDGE);
	  (void)pollGPIO(fd15val,50);
	  timestamp1 = rdtsc();
	  edgeGPIO(fd15dir,GPIO_FALLING_EDGE);
	  (void)pollGPIO(fd15val,50);
	  timediff = (unsigned long)(rdtsc() - timestamp1);
	  dist = timediff/DIVFACTOR;
	  set_distance(dist);
	  //printf("\ntime in ms is %lu - %lu = %lu (%lu)\n\n",time2,time1,(time2 - time1),(time2 - time1)/13841);
	  usleep(400000);
	  //sleep(3);
	}
	close(fd15val);
	close(fd15dir);
	return;
}
int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	pthread_t display_thread,sensor_thread;
	struct sigaction act;

        memset (&act, '\0', sizeof(act));
        act.sa_sigaction = &terminatehdl;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGINT, &act,NULL) < 0) {
                perror ("sigaction");
                return 1;
        }

	/* initialize mutex to 1 - Semaphore for distance */
        sem_init(&mutex_distance, 0, 1);

	/* initialize all the gpio  */
	initGPIO(14,GPIO_DIRECTION_OUT);
	initGPIO(15,GPIO_DIRECTION_IN);
	initGPIO(31,GPIO_DIRECTION_OUT);
	initGPIO(30,GPIO_DIRECTION_OUT);
	initGPIO(42,GPIO_DIRECTION_OUT);
	initGPIO(43,GPIO_DIRECTION_OUT);
	initGPIO(55,GPIO_DIRECTION_OUT);
	/* initialize all the gpio  */
	writeGPIO(14,0);
	edgeGPIO(15,GPIO_RISING_EDGE);
	writeGPIO(31,0);
	writeGPIO(30,0);
	writeGPIO(42,0);
	writeGPIO(43,0);
	writeGPIO(55,0);
         
	pthread_create(&sensor_thread, NULL, sensor_task,NULL);
	pthread_create(&display_thread, NULL, display_task,NULL);
	pthread_join( sensor_thread, NULL);
	pthread_join( display_thread, NULL);	
	/*As we are done with all the threads destory the semaphores*/
	sem_destroy(&mutex_distance);             /* destroy semaphore for distance*/ 
	deinitGPIO(14);
	deinitGPIO(15);
	return ret;
}


