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
#include <signal.h>
//#include "rdtsc.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SPI_LED_TX_PATTERN       1
#define SPI_LED_INITIALISE       3
#define GPIO_RISING_EDGE  0
#define GPIO_FALLING_EDGE 1
#define GPIO_DIRECTION_OUT  0
#define GPIO_DIRECTION_IN 1

int terminate =0;
static const char *display = "/dev/spiled";
static const char *sensor = "/dev/sensor";

static void pabort(const char *s)
{
	perror(s);
	abort();
}
static void terminatehdl (int sig, siginfo_t *siginfo, void *context)
{
        printf ("Sending PID: %ld, UID: %ld\n ",
                        (long)siginfo->si_pid, (long)siginfo->si_uid);
        terminate = 1;
}
static void transfer(int fd)
{
	int ret,i,k;
	uint8_t tr[160];
	uint8_t tx0[] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        };/* Digit 1  */
	uint8_t tx1[] = {
        0x19,0xFB,0xEC,0x08,0x08,0x0F,0x09,0x10
        };/* Digit 1  */
	uint8_t tx2[] = {
        0x18,0xFF,0xE9,0x08,0x0B,0x0E,0x08,0x04
        };/* Digit 1  */
	uint8_t tx3[] = {
        0x10,0x09,0x0f,0x08,0x08,0xEC,0xFB,0x19
        };/* Digit 1  */
	uint8_t tx4[] = {
        0x04,0x08,0x0E,0x0B,0x08,0xE9,0xFF,0x18
        };/* Digit 1  */
	memset(tr,0,80);
	memcpy(tr,tx0,8);
	memcpy((tr+8),tx1,8);
	memcpy((tr+16),tx2,8);
	memcpy((tr+24),tx3,8);
	memcpy((tr+32),tx4,8);
	ret = ioctl(fd, SPI_LED_TX_PATTERN, &tr);
	if (ret < -1)
		pabort("can't send spi message");
        sleep(1);
}
void transfer_moves(int fd)
{
	uint8_t tmp[8];
	uint8_t val;
	int ret,i;
	uint8_t tr[80];
	memset(tr,0,80);
	memset(tmp,0,8);
	tmp[4] = 0xff;
	tmp[5] = 0xff;
	memcpy(tr,tmp,8);
	memset(tmp,0,8);
	for(i =0;i<8;i++)
	{
	  tmp[i] = 0x18;
	}
	memcpy((tr+8),tmp,8);
	memset(tmp,0,8);
	val =1;
	for(i =7;i<=0;i--)
	{
	  tmp[i] = val;
	  val = val*2;
	}
	memcpy((tr+16),tmp,8);
	memset(tmp,0,8);
	val =1;
	for(i =0;i< 8;i++)
	{
	  tmp[i] = val;
	  val = val*2;
	}
	memcpy((tr+24),tmp,8);
	ret = ioctl(fd, SPI_LED_TX_PATTERN, &tr);
	if (ret < -1)
		pabort("can't send spi message");
        sleep(1);
}
void anime(int fd)
{
	unsigned int wdata[8],i;

	for(i = 0;i < 4;i++)
	{
	  wdata[(2*i)] = i;
 	  wdata[(2*i+1)] = 103;
	}
	for(i =0;i<10;i++)
	{
	  write(fd,wdata,sizeof(wdata));
	 // write(fd,wdata,sizeof(wdata));
	}
	return;
}
void move_left(int fd)
{
	unsigned int wdata[4];

	wdata[0] = 1;
	wdata[1] = 103;
	wdata[2] = 2;
	wdata[3] = 103;
	write(fd,wdata,sizeof(wdata));
	write(fd,wdata,sizeof(wdata));
	return;
}
void move_right(int fd)
{
	unsigned int wdata[4];

	wdata[0] = 3;
	wdata[1] = 203;
	wdata[2] = 4;
	wdata[3] = 203;
	write(fd,wdata,sizeof(wdata));
	 return;
}
int main(int argc, char *argv[])
{
	int ret = 0,cnt =0;
	int fd,fds,i,diff =0;
	unsigned int dist,prevdist = 0,prev_direction;
        struct sigaction act;
	fd = open(display, O_RDWR);
	if (fd < 0)
		pabort("can't open device");
	printf("\n initialising....:");
	ioctl(fd, SPI_LED_INITIALISE, 0);
	transfer(fd);
	fds = open(sensor, O_RDWR);
	if (fds < 0)
		pabort("can't open sensor");

        memset (&act, '\0', sizeof(act));
        act.sa_sigaction = &terminatehdl;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGINT, &act,NULL) < 0) {
                perror ("sigaction");
                return 1;
        }

	while((!terminate)&&(cnt < 120)){	
	  dist = read(fds,NULL,0);
	  write(fds,NULL,0);
	  printf("\ndistance is %lu \n",dist);
	  diff  = dist - prevdist;
	  if(diff<0)
	  {
	    diff = prevdist - dist;
	  }
	  if(dist < 40)
	  {
	      move_left(fd);
          }
	  else 
	  {
	      move_right(fd);
          }
	  prevdist = dist;
	  cnt++;
	}
	write(fd,NULL,0);
	/* time to displat some patterns */
	transfer_moves(fd);
	anime(fd);	
	write(fd,NULL,0);
	usleep(100000);
	close(fd);
	close(fds);
	return 0;
}


