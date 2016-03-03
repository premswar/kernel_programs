1. This directory consists of 5 files kalman.c  kalman.h   main4.c  Makefile  MPU6050_defines.h 
2. kalman.c has the implementation of kalman filter and kalman.h is its header file
3. MPU6050_defines.h has all the register address that needed to access the IMU MPU6050
4. main4.c has the main program. Change the IPADDR defined in main.c MACROS to IPADDR where unity is running
5. Issue make it will generate main4 application and run it on the gallileo 
6. The Sampling frequency for the main4.c is configured to 200Hz. 
7. The maximum attainable sampling frequency is measured to be 400Hz. This done by running the program for 2 sec using sleep function and then noted the number of interrupts. after 400HZ the number interrupts are not same as configured sampling frequency.
8.TCP control(unity program) version2 is used.