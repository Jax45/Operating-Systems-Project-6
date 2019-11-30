//Name: Jackson Hoenig
//Class: CMP-SCI 4760
/*Description:
 *
 *     This user process is called from the OSS as a child process. its purpose is to request memory pages from its page
 *     table in its Process control block. the request is randomly calculated to be a read or write request.
 *     if it is a write it sends back the appropriate message as with the read request.
 *     if it is a read or write request a random number from 0 to 32k is calculated to simulate the address.
 *     then that address is sent back and a message is waited upon for that request to be fulfilled.
 *     after 1000 requests granted it checks to see if it randomly terminates.
 * 
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
	//int shmid = atoi(argv[1]);	
	int semid = atoi(argv[2]);
	int msgid = atoi(argv[3]);
	//int shmidPID = atoi(argv[4]);
	//int shmidpcb = atoi(argv[5]);
	int bitIndex = atoi(argv[6]);
//	int shmidrd = atoi(argv[7]);	
//        int bound = atoi(argv[8]);
	if(semid == -1){
                perror("Error: user: Failed to create a private semaphore");
                exit(1);
        }	
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	
	int choice;

	//start at 1 to prevent premature termination. 0 % 1000 is 0
	int requests = 1;
    while(1){	
	
	//get the address from 0 to 32k for the 32 pages
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
