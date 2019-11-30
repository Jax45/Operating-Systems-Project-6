//Name: Jackson Hoenig
//Class: CMP-SCI 4760
/*Description:

When the user is forked off, it first calculates a time that it will run
an event based on the shared memory clock and the bound given in the options of OSS.
when that event time passes a number from 0-2 is generated(0 only possible after 1 second)
0 means terminate and a message is sent to OSS to let it know.
1 means request and it awaits its request granted by waiting on shared memory.
2 means release and it waits on the shared memory for the OSS to tell it
that is is done releasing memory. then the process loops until a 0 is rolled or
the OSS kills it.

NOTE: to make the project look more life like the 0 is forced if the process
has received all of its max claims of resources.
*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/msg.h>
#include <stdbool.h>
#include "customStructs.h"
#include "semaphoreFunc.h"
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define NUM_RES 20

int main(int argc, char  **argv) {
	srand(getpid());
	struct sembuf semsignal[1];
	struct sembuf semwait[1];
	//int error;	
	//get IPC data.	
	int shmid = atoi(argv[1]);	
	int semid = atoi(argv[2]);
	int msgid = atoi(argv[3]);
	int shmidPID = atoi(argv[4]);
	int shmidpcb = atoi(argv[5]);
	int bitIndex = atoi(argv[6]);
//	int shmidrd = atoi(argv[7]);	
//        int bound = atoi(argv[8]);
	if(semid == -1){
                perror("Error: user: Failed to create a private semaphore");
                exit(1);
        }	
	//initialize purpose
	int purpose = 0;
	bool isFinished = false;
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	
	int choice;

	//start at 1 to prevent premature termination. 0 % 1000 is 0
	int requests = 1;
    while(1){	
	
	//generate random time to execute event
/*	int executeMS = rand() % 1000;//% bound + 1;
	struct Clock execute;
	execute.nano = 0;
	execute.second = 0;
	r_semop(semid, semwait, 1);
        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	//execute.nano += shmclock->nano;
	execute.second += shmclock->second;
	
	long long exeNS = (executeMS * 1000000) + shmclock->nano;
	if(exeNS > 1000000000){
		execute.second += (exeNS / 1000000000);
		execute.nano += exeNS % 1000000000;
	}
	else{
		execute.second = shmclock->second;
		execute.nano = exeNS;
	}
	//execute.nano += shmclock->nano;
	shmdt(shmclock);
	r_semop(semid, semsignal, 1);
	// 0 - 250 ms
		

	int cpu = 0;
	fflush(stdout);
	bool isWaiting = true;
	while(isWaiting){
		r_semop(semid, semwait, 1);
        	shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
        	struct PCB *shmpcb = (struct PCB*) shmat(shmidpcb, (void*)0,0);
		finished = true;
	
		if((execute.second == shmclock->second && execute.nano < shmclock->nano) || execute.second < shmclock->second){
			
			isWaiting = false;

			cpu += executeMS;
			if( cpu > 1000){
				isFinished = true;
			}
		}
		shmdt(shmpcb);	
		shmdt(shmclock);
		r_semop(semid, semsignal, 1);
	}
*/	
	//get the address
	unsigned int address = rand() % 32767;
	
	if(requests % 1000 == 0){
		choice = rand() % 3;	
	}
	else{
		choice = rand() % 2;
	}
	//choice = 0;
	if(choice == 0){
		//request for read
		requests++;
		
		//printf("Sending message");
		//send message
		message.mesg_type = 1;
		message.pid = getpid();
		sprintf(message.mesg_text, "Read");
		message.page = address >> 10;
		msgsnd(msgid, &message, sizeof(message), 0);
	        message.bitIndex = bitIndex;

		//receive message
		while(1){
			int result = msgrcv(msgid, &message, sizeof(message), getpid(), 0);
			if(result != -1 && message.pid == getpid()){
				break;
			}
		}
	}
	else if (choice == 1){
		//request for write
		requests++;
		message.mesg_type = 1;
                message.page = address;// >> 10;
                message.pid = getpid();
		message.bitIndex = bitIndex;
                sprintf(message.mesg_text, "Write");
                msgsnd(msgid, &message, sizeof(message), 0);
		while(1){
                        int result = msgrcv(msgid, &message, sizeof(message), getpid(), 0);
                        if(result != -1 && message.pid == getpid()){
                                break;
                        }
                }

		
	}
	else {
		//termination
		message.mesg_type = 1;
		message.pid = getpid();
                message.bitIndex = bitIndex;
                sprintf(message.mesg_text, "Done");
                msgsnd(msgid, &message, sizeof(message), 0);
		exit(1);

	}
	/*if(false &&(strcmp(message.mesg_text,"Page Fault")) == 0){
		bool isWaiting = true;
		bool firstTime = true;
		struct Clock execute;
	        while(isWaiting){
        	        r_semop(semid, semwait, 1);
                	struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
                	struct PCB *shmpcb = (struct PCB*) shmat(shmidpcb, (void*)0,0);
			if(firstTime){
				execute.second = shmclock->second;
				execute.nano = shmclock->nano;
				//add 14 ms
				//execute.nano += 14000000;
				if (execute.nano >= 1000000000 - 14000000){
					execute.second += execute.nano / 1000000000;
					execute.nano = execute.nano % 1000000000;
				}
			}
	                if((execute.second == shmclock->second && execute.nano < shmclock->nano) || execute.second < shmclock->second){
	                        isWaiting = false;
	                }
	                shmdt(shmpcb);
	                shmdt(shmclock);
	                r_semop(semid, semsignal, 1);
		}
        }*/
	//continue back.	
    }
    return 0;
}	
