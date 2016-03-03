1. driver_dynamic folder contains dynamically allocated driver(squeue.c) and application program(test_program.c) to test with prints
2. driver_static folder contains statically allocated driver(squeue.c) and application program(test_program.c) to test with out prints
3. Enter the required directory and issue make . kernal module and testing app are generated

4. insert the kernal module squeue.ko into kernal (usage insmod squeue.ko)

5. run the application 'TestingApp' (./TestingApp)

6. The program will send messages for 10 secs and finishes after all the messages are recived

7. the send message details and recived message details allong with accumulated times are displayed on console ( not displayed for static case).

8. To remove the kernal module rmmod squeue.ko

9. To change the time program needs to send messages open test_program.c, change the MACRO MSG_SENDING_TIME to the desired value in secs. Now recompile and repeat the teps from 2.

10. Perfreport doc is the report of perftool statstics for both programs run on ubuntu