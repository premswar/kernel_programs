#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>  /* Semaphore */

#define NUM_TASKS         7
#define NUM_QUEUES        4
#define IN_QUEUE          0
#define NUM_SENDERS       3
#define NUM_RECVRS        3
#define MSG_SENDING_TIME  10  
#define SUCCESS           0

void *bus_deamon();
void *recvr_task();
void *sendr_task();

char *queue_name[NUM_QUEUES] = 
{
  "bus_in_q",
  "bus_out_q1",
  "bus_out_q2",
  "bus_out_q3"
};

/* message structure */
struct msg 
{
   int sid;               /* Source Id */
   int did;               /* Destination Id */
   int mid;               /* Message Id */
   int enque_time;        /* Enqueue time stamp*/
   int accum_time;        /* accumulated time in queue */
   char msg_buffer[80];   /* user define msg buff*/
} ;

/*thread params*/
struct thread_param 
{
   char name[20];     /*Thread name*/
   int thrdno;        /*Thread no ie.. sender1 or 2..*/
};

static int global_sequence =0;
static int total_messages_sent =0;
static int total_messages_rcvd =0;
static time_t init_time = 0;
sem_t mutex;
sem_t mutex_sent_msgcnt;
sem_t mutex_recv_msgcnt;

int main()
{
   pthread_t thread[7];
   int i = 0;
   int  iret;
   struct thread_param tp[NUM_TASKS];

    /* initialize mutex to 1 - binary semaphore */
    sem_init(&mutex, 0, 1);
    /* initialize mutex to 1 - binary semaphore */
    sem_init(&mutex_sent_msgcnt, 0, 1);
    /* initialize mutex to 1 - binary semaphore */
    sem_init(&mutex_recv_msgcnt, 0, 1);

    /*Time when program started running*/
    init_time = time(NULL);

    /*Create a bus deamon thread */
    iret = pthread_create( &thread[0], NULL, bus_deamon,NULL);
    if(iret != SUCCESS)
    {
       printf("\n Error!!Task creation failed for task : 0 with return value:%d\n",iret);
    }
   // printf("\n crated bus deamon thrd :%x",thread[0]);
   /* Create 3 independent sender threads each of which will execute function*/ 
    for (i = 1; i <= NUM_SENDERS; i++)
    {
      memset(&tp[i],0,sizeof(struct thread_param));
      tp[i].thrdno = i;
      iret = pthread_create( &thread[i], NULL, sendr_task,&tp[i]);
      if(iret != SUCCESS)
      {
         printf("\n Error!!Task creation failed for Sendr task :%d with return value:%d\n",i,iret);
      }
    //printf("\n crated bus sndr thrd :%x",thread[i]);
   }
   /* Create 3 independent rcvr threads each of which will execute function */
   for (i = 1; i <= NUM_RECVRS; i++)
   {
      memset(&tp[i+NUM_SENDERS],0,sizeof(struct thread_param));
      tp[i+NUM_SENDERS].thrdno = i;
      iret = pthread_create( &thread[i+NUM_SENDERS], NULL, recvr_task,&tp[i+NUM_SENDERS]);
      if(iret != SUCCESS)
      {
         printf("\n Error!!Task creation failed for Rcvr task :%d with return value:%d\n",i,iret);
      }
    //printf("\n crated bus rcvr thrd :%x",thread[i+NUM_SENDERS]);
   }
   /* Wait for all the threads to join */
   for (i = 0; i < NUM_TASKS; i++)
   {
      pthread_join( thread[i], NULL);
   }
    //printf("\n exiting \n");
   sem_destroy(&mutex); /* destroy semaphore */     
   return 0;
}

void nap()
{
  unsigned int usecs;

  usecs = (((rand()%10)+1)*1000);
  //printf("\n sleeping for %d usecs \n",usecs);
  usleep(usecs);
  return;
}
int get_uptime()
{
  time_t crnt_time;
  crnt_time = time(NULL);
  return (crnt_time - init_time);
}
int get_message_id()
{
  int message_id;
  sem_wait(&mutex);       /* down semaphore */
  message_id = global_sequence++;
  sem_post(&mutex);       /* up semaphore */
  return message_id;
}
void sent_message_cnt_update()
{
   sem_wait(&mutex_sent_msgcnt);       /* down semaphore */
   total_messages_sent++;
   sem_post(&mutex_sent_msgcnt);       /* up semaphore */
   return;
}
int sent_message_cnt_get()
{
   int cnt;
   sem_wait(&mutex_sent_msgcnt);       /* down semaphore */
   cnt = total_messages_sent;
   sem_post(&mutex_sent_msgcnt);       /* up semaphore */
   return cnt;
}
void recv_message_cnt_update()
{
   sem_wait(&mutex_recv_msgcnt);       /* down semaphore */
   total_messages_rcvd++;
   sem_post(&mutex_recv_msgcnt);       /* up semaphore */
   return;
}
int recv_message_cnt_get()
{
   int cnt;
   sem_wait(&mutex_recv_msgcnt);       /* down semaphore */
   cnt = total_messages_rcvd;
   sem_post(&mutex_recv_msgcnt);       /* up semaphore */
   return cnt;
}
void *bus_deamon()
{
   int fd[NUM_QUEUES];
   int res,i;
   char buff[1024];
   int msgcnt =0;
   char queueName[20];
   struct msg *message;
   
   //printf("\n this is bus_deamon thrd \n");
   /* Open all queues so that it can perform routing*/
   for(i = 0; i< NUM_QUEUES; i++)
   { 
      memset(queueName,0,20);
      sprintf(queueName,"/dev/%s",queue_name[i]);
      fd[i] = open(queueName, O_RDWR);
      if (fd[i] < 0 )
      {
         printf("\n Can not open device file : %s.\n",queueName);
         return ;
      }
   }
   while(1)
   {
      memset(buff, 0, 1024);
      res = read(fd[IN_QUEUE], buff, sizeof(struct msg));
      if(res == -1)
      {
//         printf("\n bus deamon failed to read from inqueue\n");
         nap();/*mean while sender thread might right the data*/
         continue;
      }
      nap();
      /*Read the destination in msg and send it to the respective out queue*/
      message = (struct msg *)buff;
//      printf("\n bus  read from inqueue mid:%d sid:%d did:%d,len:%d,size:%d\n",message->mid,message->sid,message->did,strlen(buff)+1,sizeof(struct msg));
      while(1)
      {
         res = write(fd[message->did], buff, sizeof(struct msg));
         if(res == -1)
         {
  //          printf("\n bus deamon thread write failed queue full for did:%d\n",message->did);
            nap();
            continue;
         }
       //  printf("\n bus deamon write to out queue :%d\n",message->did);
         break;
      }
      msgcnt++;
      if(get_uptime() > MSG_SENDING_TIME)
      {
         if(msgcnt == (sent_message_cnt_get()))
         {
            printf("\n done with routing !! bus demon exit!!:%d\n",msgcnt);
            break;
         }
      }
      nap();/* just to run it little slow*/
   }
   /* we are done close all queues and return*/
   for(i = 0; i< NUM_QUEUES; i++)
   {
      close(fd[i]);
   }
   //printf("\n bus deamon exiting");
   return;
}

void *recvr_task(void *ptr)
{
   int fdout,res;
   int id;
   char buff[256];
   int msgcnt = 0;
   char queueName[20];
   struct msg *message;

   id = ((struct thread_param*) ptr)->thrdno;
   //printf("\n This reciver task %d \n",id);//debug print remove later
   sprintf(queueName,"/dev/%s",queue_name[id]);
   fdout = open(queueName, O_RDWR);
   if (fdout < 0)
   {
      printf("Can not open device file:%s.\n",queueName);
      return ;
   }
   while(1)
   {
      if(get_uptime() > MSG_SENDING_TIME)
      {
         if((recv_message_cnt_get() == sent_message_cnt_get()))
         {
            break;
         }
      }
      memset(buff, 0, 256);
      res = read(fdout, buff, 256);
      if(res == -1)
      {
    //     printf("\n read failed from out queue:%d",id);
         nap();
         continue;
      }
      message = (struct msg *)buff;
/*      printf("\n Reading Message Id :%d Sid :%d Did :%d accumTime :%d ms\n",message->mid,
                 message->sid,message->did,(((message->accum_time)/400)/1000));*/
      msgcnt++;
      recv_message_cnt_update();
      nap();/*to slow down the operation*/
   }
   close(fdout);
}
void *sendr_task(void *ptr)
{
   int fdin,res;
   int id;
   char queueName[20];
   struct msg message;

   id = ((struct thread_param*) ptr)->thrdno;
   //printf("\n This sender task %d \n",id);//debug print remove later
   sprintf(queueName,"/dev/%s",queue_name[IN_QUEUE]);
   fdin = open(queueName, O_RDWR);
   if (fdin < 0)
   {
      printf("Can not open device file:%s.\n",queueName);
      return ;
   }

   while(1)
   {
      memset(&message, 0, sizeof(struct msg));
      /*frame messsage*/
      message.sid = id;
      message.did = id;
      message.mid = get_message_id();
      message.enque_time = 0;
      message.accum_time = 0;
      sprintf(message.msg_buffer,"This is message body :%d",rand());
      //printf("\n This sender task %d writing to inqueue\n",id);//debug print remove later
      res = write(fdin, (char*)&message, sizeof(struct msg));
      if(res == -1)
      {
         //   printf("Can not write to the device file.\n");
         nap();
         continue;
      }
      /*printf("\n Sent Message Id :%d Sid :%d Did :%d accumTime :%d ms\n",message.mid,
                 message.sid,message.did,(((message.accum_time)/400)/1000));*/
      sent_message_cnt_update();
      nap();/*to slow down the operation*/

      if(get_uptime() > MSG_SENDING_TIME)
      {
         printf("\n Times up exiting (%d):tcnt:%d\n",id,sent_message_cnt_get());
         break;
      }
   }
   //printf("\n This sender task %d exiting\n",id);//debug print remove later
   close(fdin);
}
