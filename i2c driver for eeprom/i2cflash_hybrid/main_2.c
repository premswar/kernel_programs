#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#define PAGE_SIZE          64
#define PAGE_COUNT         10
#define	FLASHGETS          3                     /*ioctl command to get status if eeprom busy or not   */
#define	FLASHGETP	   4	                 /*ioctl command to get status of current page pointer */
#define	FLASHSETP          5	                 /*ioctl command to set status of current page pointer */
#define	FLASHERASE	   6	                 /*ioctl command to erase all memory in eeprom         */

int main()
{
	char msg[PAGE_SIZE * PAGE_COUNT];
	int fd,res,i;
	unsigned long pagelocation;

	/*-----------------------------------------------------------------------------
	 *  OPEN THE DRIVER FILE - i2c_flash
	 *-----------------------------------------------------------------------------*/
	fd = open("/dev/i2c_flash", O_RDWR);
        if (fd < 0 )
        {
           printf("\n Can not open device file : i2c-0.\n");
           return 0;
        }

	/*-----------------------------------------------------------------------------
	 *  GET  THE PAGE LOCATION
	 *-----------------------------------------------------------------------------*/
        ioctl(fd,FLASHGETP,&pagelocation);
        printf("\n The current Page location in EEPROM is :%lu \n",pagelocation);

	/*-----------------------------------------------------------------------------
	 *   SET THE PAGE LOCATION
	 *-----------------------------------------------------------------------------*/
        printf("\n Setting the page location in EEPROM to : 505 \n");
        ioctl(fd,FLASHSETP,505); 
        usleep(10000); /* The write may take 10ms hence wait*/

	/*-----------------------------------------------------------------------------
	 *  GET  THE PAGE LOCATION AFTER SETTING TO VERIFY
	 *-----------------------------------------------------------------------------*/
        ioctl(fd,FLASHGETP,&pagelocation);
        printf("\n The new Page location  is :%lu",pagelocation);

	/*-----------------------------------------------------------------------------
	 *  WRITE THE MESSAGE
	 *-----------------------------------------------------------------------------*/
        memset(msg, 0, (PAGE_SIZE * PAGE_COUNT));
        for (i = 0;i <PAGE_COUNT; i++)
        {
           sprintf((msg+(i*64)),"Hi this is Message - %d prem:%d",i,(i+5000));
	   printf("\n writing : %s\n",(msg+(i*64)));
        }
        res = write(fd, msg, PAGE_COUNT);
        if(res >= 0)
        {
           printf("\n write to EEPROM SUCESS\n");
        }
        else
        {
           printf("\n write failed errno:%d :%s\n",errno,strerror(errno));
        }

	/*-----------------------------------------------------------------------------
	 *  CHECK THE STATUS OF EEPROM: as we submitted the write request
	 *-----------------------------------------------------------------------------*/
        printf("\n Checking for EEPROM status....");
        res = ioctl(fd,FLASHGETS,0);/* GET THE status of EEPROM */
        if(res <= 0)
        {
           printf("\n The status of EEPROM is:%s (%d)\n",strerror(errno),errno);
	   printf("\n As the EEPROM IS busy waiting for some time....");
           sleep(1);/* As EEPROM IS BUSY sleep for while */
        }
        else
        {
           printf("\n EEPROM is Ready !!\n");
        }

	/*-----------------------------------------------------------------------------
	 *  CHECK THE STATUS OF EEPROM: as write might finished
	 *-----------------------------------------------------------------------------*/
        printf("\n Checking for EEPROM status....");
        res = ioctl(fd,FLASHGETS,0);/* GET THE status of EEPROM */
        if(res <= 0)
        {
           printf("\n The status of EEPROM is:%s (%d)\n",strerror(errno),errno);
        }
        else
        {
           printf("\n EEPROM is Ready !!\n");
        }
	/*-----------------------------------------------------------------------------
	 *  GET  THE PAGE LOCATION
	 *-----------------------------------------------------------------------------*/
        ioctl(fd,FLASHGETP,&pagelocation);
        printf("\n The current Page location in EEPROM is :%lu \n",pagelocation);


	/*-----------------------------------------------------------------------------
	 *  READ THE DATA: first we need to reset the pointer to initial write location
	 *  and submit the read request
	 *-----------------------------------------------------------------------------*/
        printf("\n The Setting the Page location in EEPROM to 505 \n");
        ioctl(fd,FLASHSETP,505); 
        usleep(10000);
        memset(msg, 0, (PAGE_SIZE * PAGE_COUNT));
	/*-----------------------------------------------------------------------------
	 *  READ UNTIL THE DATA IS READY
	 *-----------------------------------------------------------------------------*/
        while(1)
        {
           res = read(fd, msg, PAGE_COUNT);
           if(res >= 0)
           {
              printf("\n Read sucess:\n");
              for(i = 0;i <PAGE_COUNT; i++)
              {
                printf("\n MSGS are %d:%s\n",i,(msg+(i*64)));
              }
              break;
           }
           else
           {
             printf("\n The EEPROM is BUSY [%s (%d)]\n",strerror(errno),errno);
           }
           usleep(30000);
        }

	/*-----------------------------------------------------------------------------
	 *  CHECK THE STATUS OF EEPROM: as we submitted the read request
	 *-----------------------------------------------------------------------------*/
	printf("\n checking the status of EEPROM...");
        res = ioctl(fd,FLASHGETS,0);/* GET THE status of EEPROM */
        if(res <= 0)
        {
           printf("\n The status of EEPROM is:(%d) %s\n",errno,strerror(errno));
        }
        else
        {
           printf("\n EEPROM is Ready (%d)\n",res);
        }

	/*-----------------------------------------------------------------------------
	 *  ERASE ALL THE MEMORY IN EEPROM
	 *-----------------------------------------------------------------------------*/
        printf("\n Erasing total memory of EEPROM CHECK LED iS ON..\n");
        ioctl(fd,FLASHERASE,0);/* erase operation */
        printf("\n Erased ...");

	/*-----------------------------------------------------------------------------
	 *  READ UNTIL THE DATA IS READY
	 *-----------------------------------------------------------------------------*/
        printf("\n Reading data after Erase");
        while(1)
        {
           memset(msg, 0, (PAGE_SIZE * PAGE_COUNT));
           res = read(fd, msg, 1);
           if(res >= 0)
           {
              printf("\n Read sucess:\n");
              for(i = 0;i <1; i++)
              {
                 printf("\n MSGS are %d:%s\n",i,(msg+(i*64)));
              }
              break;
           }
           else
           {
              if(errno == EAGAIN)
              {
                 printf("\n Read Reuested submitted [%s]\n",strerror(errno));
              }
              else
              {
                 printf("\n The EEPROM is BUSY [%s (%d)]",strerror(errno),errno);
              }
           }
           usleep(30000);
        }

	/*-----------------------------------------------------------------------------
	 *  TESTED ALL POSSIBLE CASES CLOSE
	 *-----------------------------------------------------------------------------*/
        close(fd);
        printf("\n Tested all possible cases sucessfully Exiting \n\n");
        return 0;
}
