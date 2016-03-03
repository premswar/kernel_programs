1. We have two folders i2cflash_nonblocking, i2cflash_hybrid and Word document discussing about possible explanations.
2. change your directory to i2cflash_nonblocking
3. issue make command
4. insert the i2c_flash.ko module 
5. Run the Test program (./TestingApp)
6. observe all the output(testing all combinations of driver).
   It is non blocking as the read function returns and wait for data to be loaded by kernel 
++++++++++++++++++++++++++++++++++++++++++
7. change your directory to i2cflash_hybrid
8. issue make command
9. insert the i2c_flash.ko module
10.Run the Test program (./TestingApp)
11.Observe the output its is blocking calls. read function waits and returns after printing data. 
12.Edit the i2c_flash.c file change the Macro NON_BLOCKING to 1 and save the file.
13.issue make command and remove the previous module
14 repeat the steps 9,10
15 observe the output. It is non blocking as the read function returns and wait for data to be loaded by kernel 
( Note: When The program run on freshly booted Galileo there is trace observed for gpio call which is harmless. 
       No solution is updated in discussion board to this trace as some problem with gpio lib scripts. No harm just trace)