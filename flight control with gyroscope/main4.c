/*
 * =====================================================================================
 *
 *       Filename:  read_data.c
 *
 *    Description: Main function that reads data from gyro and  
 *                 applies kalman filter and send to unity program
 *        Version:  1.0
 *        Created:  11/18/2014 01:48:32 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *   NOTES:some part of code is ported from 
 *   https://github.com/richardghirst/PiBits/tree/master/MPU6050-Pi-Demo
 * =====================================================================================
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <poll.h>
#include "MPU6050_defines.h"
#include "kalman.h"

#define I2C_DEV "/dev/i2c-0"
#define IPADDR  "169.254.202.152"
#define PORT     9999
#define SENSOR_INTR         15
#define I2C_MUX             29
#define INTR_MUX            30
#define gyro_xsensitivity   131  /* for +- 250 config sensitivity is 131lsb per deg*/
#define gyro_ysensitivity   131
#define gyro_zsensitivity   131
#define PI                  3.14
#define RAD_TO_DEG          (180/PI)
#define DEG_TO_RAD          (PI/180)
int i2c_fd,fd_intr;
int GYRO_XOUT_OFFSET=0, GYRO_YOUT_OFFSET=0, GYRO_ZOUT_OFFSET=0;
struct Kalman kalmanX,kalmanY;
float accelerationx[2], accelerationy[2], accelerationz[2];
float velocityx[2], velocityy[2],velocityz[2];
float positionX[2];
float positionY[2];
float positionZ[2]; 
void Calibrate_Gyros();
int connection_init(char *ipAddr,int port);
int send_to_unity(int fd,float x,float y,float z,float wx,float wy,float wz);
//extern void connection_destroy(int fd);
sig_atomic_t terminate =0;

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
void MPU_Write(uint8_t addr, uint8_t offset, uint8_t value)
{
    uint8_t physical_addr = addr ;
    uint8_t setaddr = offset;
    unsigned char buf[10];
 
    memcpy(buf,&setaddr,sizeof(setaddr));
    memcpy((buf+1),&value,sizeof(value));
    if(!ioctl(i2c_fd, I2C_SLAVE_FORCE, physical_addr))
    {
        if(write(i2c_fd, buf, 2)<0)
        printf("write value is %x:%s\n",value,strerror(errno));
    }
   else
   { 
      printf("ioctl failed for %d:%s\n",I2C_SLAVE,strerror(errno));
   }

}

void MPU_Read(uint8_t addr, uint8_t offset, uint8_t *value)
{
    unsigned long physical_addr =  addr;
    uint8_t setaddr = offset;

    if(!ioctl(i2c_fd, I2C_SLAVE_FORCE, physical_addr))
    {
        write(i2c_fd, &setaddr, sizeof(setaddr));
        read(i2c_fd, value, sizeof(*value));
        //printf("read value is %x:%s\n",*value,strerror(errno));
    }
   else
   { 
      printf("ioctl failed for %d:%s\n",I2C_SLAVE,strerror(errno));
   }
}
void MPU_Read_Sensor(uint8_t *sensor_data)
{
    unsigned long physical_addr =  MPU6050_ADDRESS;
    uint8_t setaddr = MPU6050_RA_ACCEL_XOUT_H;
    struct pollfd fdset[1];
    int rc,value;
    //fd_set readset;
    fdset[0].fd = fd_intr;
    fdset[0].events = POLLPRI;
    if(!ioctl(i2c_fd, I2C_SLAVE_FORCE, physical_addr))
    {	
	rc = poll(fdset,1,-1);
	if(rc < 0)
	{
	   printf("\n poll failed:%s\n",strerror(errno));
	   return;
	}
        // read value
        if(fdset[0].revents & POLLPRI)
	{
	  //printf("GPIO interrupt happened:%d\n",rc);
	  pread(fd_intr,&value,1,0);//acknowledge the interrupt
          write(i2c_fd, &setaddr, sizeof(setaddr));
          read(i2c_fd, sensor_data, 14);
	}
        //printf("read value is %x:%s\n",*value,strerror(errno));
    }
   else
   { 
      printf("ioctl failed for %d:%s\n",I2C_SLAVE,strerror(errno));
   }
}
void MPU_SETUP()
{
    //Sets clock source to gyro reference w/ PLL
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_PWR_MGMT_1, 0b00000010);

    //Controls frequency of wakeups in accel low power mode plus the sensor standby modes
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_PWR_MGMT_2, 0x00);

    //Sets sample rate to 8000/1+39 = 200Hz
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_SMPLRT_DIV, 0x27);

    //Disable FSync, 256Hz DLPF
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_CONFIG, 0x00);

    //Disable gyro self tests, scale of 250 degrees/s
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_GYRO_CONFIG, 0b00000000);

    //Disable accel self tests, scale of +-2g, no DHPF
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_ACCEL_CONFIG, 0x00);

    //Freefall threshold of |0mg|
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_FF_THR, 0x00);

    //Freefall duration limit of 0
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_FF_DUR, 0x00);

    //Motion threshold of 0mg
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_MOT_THR, 0x00);

    //Motion duration of 0s
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_MOT_DUR, 0x00);

    //Zero motion threshold
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_ZRMOT_THR, 0x00);

    //Zero motion duration threshold
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_ZRMOT_DUR, 0x00);

    //Disable sensor output to FIFO buffer
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_FIFO_EN, 0x00);
 
    //AUX I2C setup
    //Sets AUX I2C to single master control, plus other config
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_MST_CTRL, 0x00);
    //Setup AUX I2C slaves
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV0_ADDR, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV0_REG, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV0_CTRL, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV1_ADDR, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV1_REG, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV1_CTRL, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV2_ADDR, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV2_REG, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV2_CTRL, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV3_ADDR, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV3_REG, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV3_CTRL, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV4_ADDR, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV4_REG, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV4_DO, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV4_CTRL, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV4_DI, 0x00);
 
    //Setup INT pin and AUX I2C pass through
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_INT_PIN_CFG, 0x00);
    //Enable data ready interrupt
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_INT_ENABLE, 0x01);
 
    //Slave out, dont care
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV0_DO, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV1_DO, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV2_DO, 0x00);
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_SLV3_DO, 0x00);
    //More slave config
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_I2C_MST_DELAY_CTRL, 0x00);
    //Reset sensor signal paths
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_SIGNAL_PATH_RESET, 0x00);
    //Motion detection control
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_MOT_DETECT_CTRL, 0x00);
    //Disables FIFO, AUX I2C, FIFO and I2C reset bits to 0
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_USER_CTRL, 0x00);
    //Data transfer to and from the FIFO buffer
    MPU_Write(MPU6050_ADDRESS, MPU6050_RA_FIFO_R_W, 0x00);
    //MPU6050_RA_WHO_AM_I             //Read-only, I2C address
 
    printf("\nMPU6050 Setup Complete\n");

}
int connection_init(char *ipAddr,int port)
{
    int sockfd = 0;
    struct sockaddr_in serv_addr; 
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 

    if(inet_pton(AF_INET, ipAddr, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed :%s\n",strerror(errno));
       return -1;
    }
    printf("\n connection estalblished\n");
    fflush(stdout);
    return sockfd;
}
int send_to_unity(int sockfd,float x,float y,float z,float wx,float wy,float wz)
{
    char sendBuff[256];
    int n;
    memset(sendBuff, '\0',256);
    //random_data(sendBuff);  
    sprintf(sendBuff,"%f %f %f %f %f %f\n",x,y,z,wx,wy,wz);
    n = write(sockfd,sendBuff,strlen(sendBuff));
    //printf("\n%s:%d:%d\n",sendBuff,strlen(sendBuff),n);
    //fflush(stdout);
    //read(sockfd,sendBuff,1023);
    return 0;
}
void getPosition(float x,float y,float z)
{
   float dt = 0.05;
   accelerationx[1] = x;
   accelerationy[1] = y;
   accelerationz[1] = z;

   if ((accelerationx[1] <=3)&&(accelerationx[1] >= -3)) //Discrimination window applied
   {
     accelerationx[1] = 0;
   } // to the X axis acceleration
     //variable

   if ((accelerationy[1] <=3)&&(accelerationy[1] >= -3))
   {
      accelerationy[1] = 0;
   }
   if ((accelerationz[1] <=3)&&(accelerationz[1] >= -3))
   {
      accelerationz[1] = 0;
   }
   //first X integration:
   velocityx[1]= velocityx[0]+ (accelerationx[0]+ ((accelerationx[1] -accelerationx[0])/2))*dt;
   //second X integration:
   positionX[1]= positionX[0] + (velocityx[0] + ((velocityx[1] - velocityx[0])/2))*dt;
   //first Y integration:
   velocityy[1] = velocityy[0] + (accelerationy[0] + ((accelerationy[1] -accelerationy[0])/2))*dt;
   //second Y integration:
   positionY[1] = positionY[0] + (velocityy[0] + ((velocityy[1] - velocityy[0])/2))*dt;
   //first Z integration:
   velocityz[1] = velocityz[0] + (accelerationz[0] + ((accelerationz[1] -accelerationz[0])/2))*dt;
   //second Z integration:
   positionZ[1] = positionZ[0] + (velocityz[0] + ((velocityz[1] - velocityz[0])/2))*dt;

   accelerationx[0] = accelerationx[1]; //The current acceleration value must be sent
   //to the previous acceleration
   accelerationy[0] = accelerationy[1]; //variable in order to introduce the new
   //acceleration value.
   accelerationz[0] = accelerationz[1]; 

   velocityx[0] = velocityx[1]; //Same done for the velocity variable
   velocityy[0] = velocityy[1];
   velocityz[0] = velocityz[1];
   positionX[0] = positionX[1];
   positionY[0] = positionY[1];
   positionZ[0] = positionZ[1];
}
void gpio_init()
{
        char buffer[256];
        int fileHandle;
//#if 0

  //Export GPIO
        fileHandle = open("/sys/class/gpio/export", O_WRONLY);
        sprintf(buffer, "%d", I2C_MUX);
        write(fileHandle, buffer, strlen(buffer));
        sprintf(buffer, "%d", SENSOR_INTR);
        write(fileHandle, buffer, strlen(buffer));
        sprintf(buffer, "%d", INTR_MUX);
        write(fileHandle, buffer, strlen(buffer));
        close(fileHandle);
 
   //Direction GPIO
        sprintf(buffer, "/sys/class/gpio/gpio%d/direction", I2C_MUX);
        fileHandle = open(buffer, O_WRONLY);
        // Set out direction
        write(fileHandle, "out", 3);
        close(fileHandle);
   //Direction GPIO
        sprintf(buffer, "/sys/class/gpio/gpio%d/direction", INTR_MUX);
        fileHandle = open(buffer, O_WRONLY);
        // Set out direction
        write(fileHandle, "out", 3);
        close(fileHandle);
   //Direction GPIO
        sprintf(buffer, "/sys/class/gpio/gpio%d/direction", SENSOR_INTR);
        fileHandle = open(buffer, O_WRONLY);
        // Set in direction
        write(fileHandle, "in", 2);
        close(fileHandle);
    sprintf(buffer, "/sys/class/gpio/gpio%d/value", I2C_MUX);
    fileHandle = open(buffer, O_RDWR);
    write(fileHandle, "0", 1);
    close(fileHandle);
    sprintf(buffer, "/sys/class/gpio/gpio%d/value", INTR_MUX);
    fileHandle = open(buffer, O_RDWR);
    write(fileHandle, "0", 1);
    close(fileHandle);
    sprintf(buffer, "/sys/class/gpio/gpio%d/edge", SENSOR_INTR);
    fileHandle = open(buffer, O_RDWR);
    if (fileHandle < 0)
	pabort("can't open device");
    write(fileHandle,"rising",6);
    close(fileHandle);
//#endif
     sprintf(buffer, "/sys/class/gpio/gpio%d/value", SENSOR_INTR);
    fd_intr = open(buffer, O_RDWR);
    if (fd_intr < 0)
	pabort("can't open device");
}
int main()
{
    uint8_t sensor_data[14];
    unsigned char Data = 0x00;
    int  accX,accY,accZ;
    int  gyroX,gyroY,gyroZ;
    int  accX_accum,accY_accum,accZ_accum;
    int  gyroX_accum,gyroY_accum,gyroZ_accum;
    int x,unity_fd,kalman_enable,/*val,*/cnt =0;
    float accXangle,accYangle;
    float gyroXrate,gyroYrate,gyroZrate;
    float GYRO_XOUT_UNITY,GYRO_YOUT_UNITY,GYRO_ZOUT_UNITY;
    float GYRO_XOUT_UNITY_STORED,GYRO_YOUT_UNITY_STORED;
    float kalman_Xangle,kalman_Yangle,gyroZ_angle;
    float kalman_Xangle_old,kalman_Yangle_old,gyroZ_angle_old =0;
    float kalman_Xrate,kalman_Yrate;
    static int first_time = 1;
    struct sigaction act;

    memset (&act, '\0', sizeof(act));
    act.sa_sigaction = &terminatehdl;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGINT, &act,NULL) < 0) {
             perror ("sigaction");
             return 1;
    }

    gpio_init();
    i2c_fd = open(I2C_DEV, O_RDWR);
    if(i2c_fd <0)
    {
      printf("open failed\n");
    }

    MPU_Read(MPU6050_ADDRESS, MPU6050_RA_WHO_AM_I, &Data);
    if(Data == 0x68)
        printf("\nI2C Read Test Passed, MPU6050 Address: 0x%x", Data);
    else
    {
        printf("ERROR: I2C Read Test Failed, Stopping:0x%x \n",Data);
        return -1;
    }
    MPU_SETUP();
    unity_fd = connection_init(IPADDR,PORT); 
    if(unity_fd <0)
    {
      printf("connection to unity server failed:%s\n",strerror(errno));
        return -1;
    }
    Kalman_init(&kalmanX);
    Kalman_init(&kalmanY);
    Calibrate_Gyros();

    while(!terminate)
    {
	MPU_Read_Sensor(sensor_data);
        cnt++;
	accX = ((sensor_data[0] << 8) | sensor_data[1]);
	accY = ((sensor_data[2] << 8) | sensor_data[3]);
	accZ = ((sensor_data[4] << 8) | sensor_data[5]);
	//tempRaw = ((sensor_data[6] << 8) | sensor_data[7]);  
	gyroX = ((sensor_data[8] << 8) | sensor_data[9]);
	gyroY = ((sensor_data[10] << 8) | sensor_data[11]);
	gyroZ = ((sensor_data[12] << 8) | sensor_data[13]);
        //accX_accum += accX;gyroX_accum += gyroX;
        //accY_accum += accY;gyroY_accum += gyroY; 
        //accZ_accum += accZ;gyroZ_accum += gyroZ; 
        if(cnt%10)
        {
            continue;
        }
#if 0
        accX = accX/10;gyroX = gyroX /10;
        accY = accY/10;gyroY = gyroY /10;
        accZ = accZ/10;gyroZ = gyroZ /10;
        accX_accum = 0;gyroX_accum =0;
        accY_accum = 0;gyroY_accum =0;
        accZ_accum = 0;gyroZ_accum =0;
#endif
  // atan2 outputs the value of -π to π (radians) - see http://en.wikipedia.org/wiki/Atan2
  // We then convert it to 0 to 2π and then from radians to degrees
	accXangle = (((atan2(accY,accZ)+PI)*180)/PI);
	accYangle = (((atan2(accX,accZ)+PI)*180)/PI);
        gyroXrate = (float)(gyroX-GYRO_XOUT_OFFSET)/gyro_xsensitivity;
        gyroYrate = -((float)(gyroY-GYRO_YOUT_OFFSET)/gyro_ysensitivity);
        gyroZrate = ((float)(gyroZ - GYRO_ZOUT_OFFSET)/gyro_ysensitivity);
        kalman_Xangle = getAngle(&kalmanX,accXangle,gyroXrate,0.05);
        kalman_Yangle = getAngle(&kalmanY,accYangle,gyroYrate,0.05);
        gyroZ_angle =   (gyroZrate*0.05)+gyroZ_angle_old;
	//kalman_Xrate = getRate(&kalmanX); 
	//kalman_Yrate = getRate(&kalmanY);
	if(first_time)
	{
          kalman_Xangle_old = kalman_Xangle;
          kalman_Yangle_old = kalman_Yangle;
          gyroZ_angle_old =   gyroZ_angle;
          first_time = 0;
	}
	kalman_Xrate = (kalman_Xangle-kalman_Xangle_old)/0.05; 
	kalman_Yrate = (kalman_Yangle -kalman_Yangle_old)/0.05;
        gyroZrate    =  (gyroZ_angle - gyroZ_angle_old)/0.05;
        kalman_enable = 1;
        if(kalman_enable == 1)
        {
            gyroXrate = kalman_Xrate;
	    gyroYrate = kalman_Yrate;
        }
   	GYRO_XOUT_UNITY = ((gyroXrate*(3.14))/180);
	GYRO_ZOUT_UNITY = ((gyroZrate*(3.14))/180);
	GYRO_YOUT_UNITY = ((gyroYrate*(3.14))/180);

        kalman_Xangle_old = kalman_Xangle;
        kalman_Yangle_old = kalman_Yangle;
        gyroZ_angle_old   = gyroZ_angle;
        getPosition((accX/16384),(accY/16384),(accZ/16384));
	send_to_unity(unity_fd,0,0,0,GYRO_XOUT_UNITY,GYRO_YOUT_UNITY,GYRO_ZOUT_UNITY);
	GYRO_XOUT_UNITY_STORED += GYRO_XOUT_UNITY;
	//GYRO_YOUT_UNITY_STORED += GYRO_YOUT_UNITY;
	GYRO_YOUT_UNITY_STORED += GYRO_YOUT_UNITY;
        printf("\nGyro Xout: %f Yout: %f Zout: %f cnt:%d ",GYRO_XOUT_UNITY,GYRO_YOUT_UNITY,GYRO_ZOUT_UNITY,cnt);
        printf("\nPost Xdis: %f Ydis: %f Zdis: %f ",positionX[1],positionY[1],positionZ[1]);
    }
	/* Reset  the angular velocity */
    send_to_unity(unity_fd,0,0,0,0,0,0);
    close(i2c_fd);
    close(unity_fd);
    close(fd_intr);
    return 0;
}

void Calibrate_Gyros()
{
    uint8_t sensor_data[14];
    int  accX,accY,accZ;
    int  gyroX,gyroY,gyroZ;
    float accXangle =0,accYangle=0;
    int GYRO_XOUT_OFFSET_1000SUM =0, GYRO_YOUT_OFFSET_1000SUM =0, GYRO_ZOUT_OFFSET_1000SUM =0;
    int GYRO_XOUT =0, GYRO_YOUT= 0, GYRO_ZOUT =0;

	int x = 0;
	for(x = 0; x<10; x++)
	{
		MPU_Read_Sensor(sensor_data);
		accX = ((sensor_data[0] << 8) | sensor_data[1]);
  		accY = ((sensor_data[2] << 8) | sensor_data[3]);
  		accZ = ((sensor_data[4] << 8) | sensor_data[5]);
  		//tempRaw = ((sensor_data[6] << 8) | sensor_data[7]);  
  		gyroX = ((sensor_data[8] << 8) | sensor_data[9]);
  		gyroY = ((sensor_data[10] << 8) | sensor_data[11]);
  		gyroZ = ((sensor_data[12] << 8) | sensor_data[13]);
  
  // atan2 outputs the value of -π to π (radians) - see http://en.wikipedia.org/wiki/Atan2
  // We then convert it to 0 to 2π and then from radians to degrees
  		accXangle += (atan2(accY,accZ)+PI)*RAD_TO_DEG;
  		accYangle += (atan2(accX,accZ)+PI)*RAD_TO_DEG;
  
		GYRO_XOUT_OFFSET_1000SUM += gyroX;
		GYRO_YOUT_OFFSET_1000SUM += gyroY;
		GYRO_ZOUT_OFFSET_1000SUM += gyroZ;
		//usleep(5000);
	}	
	GYRO_XOUT_OFFSET = GYRO_XOUT_OFFSET_1000SUM/10;
	GYRO_YOUT_OFFSET = GYRO_YOUT_OFFSET_1000SUM/10;
	GYRO_ZOUT_OFFSET = GYRO_ZOUT_OFFSET_1000SUM/10;
        setAngle(&kalmanX,accXangle/10); 
        setAngle(&kalmanY,accYangle/10); 
	printf("\nGyro X offset sum: %ld Gyro X offset: %d", GYRO_XOUT_OFFSET_1000SUM, GYRO_XOUT_OFFSET);
	printf("\nGyro Y offset sum: %ld Gyro Y offset: %d", GYRO_YOUT_OFFSET_1000SUM, GYRO_YOUT_OFFSET);
	printf("\nGyro Z offset sum: %ld Gyro Z offset: %d", GYRO_ZOUT_OFFSET_1000SUM, GYRO_ZOUT_OFFSET);
        fflush(stdout);
        return ;
}


