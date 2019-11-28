/************************************************************************************
 * Name: Jackson Hoenig
 * Class: CMPSCI 4760
 * Project 5
 * Description:
 *In this program, the Operating system simulator starts by allocating shared memory
for the Resource Descriptor array of 20. about 20% of those are then declared sharable and
each class gets a total number of 1-10 that can be used. then at random time intervals
child processes are forked off. then the OSS checks to see if any messages are waiting from the
children. if not it checks to see if there are any pending requests. if there are, the
request is evaluated to be safe or not.(see Banker Algorithm below) if it is safe it is
the process is told by changing the shared memory to its child pid. finally the OSS
increments the clock and loops back.

 * *************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <errno.h>
#include <sys/msg.h>
#include "customStructs.h"
#include "semaphoreFunc.h"
#include "bitMap.h"
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define ALPHA 1
#define BETA 1
#define NUM_RES 20
//global var.
int processes = 0;
char bound[20];
bool verbose = false;
int currentSecond = 0;
//prototypes
void exitSafe(int);
bool isSafe(int,FILE*);
void printFrameTable(const struct Queue* q,FILE* fp);
//Global option values
int maxChildren = 10;
char* logFile = "log.txt";
int timeout = 3;
long double passedTime = 0.0;
bool fifo = true;

//get shared memory
static int shmid;
static int shmidPID;
static int semid;
static int shmidPCB;
static struct Clock *shmclock;
static struct PCB *shmpcb;
static struct Dispatch *shmpid;       
//get message queue id
int msgid;
                       
//array to hold the pid's of all the child processes
int pidArray[20];
char unsigned bitMap[3];
//function is called when the alarm signal comes back.
void timerHandler(int sig){
	//long double througput = passedTime / processes;
	printf("Process has ended due to timeout");//, Processses finished: %d, time passed: %Lf\n",processes,passedTime);
	//printf("OSS: it takes about %Lf seconds to generate and finish one process \n",througput);
	//printf("OSS: throughput = %Lf\n",processes / passedTime);
	//fflush(stdout);
	//get the pids so we can kill the children
	int i = 0;
	shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
        for(i = 0; i < 18; i++) {
	       if(((bitMap[i/8] & (1 << (i % 8 ))) == 0) && shmpcb[i].simPID != getpid())
		pidArray[i] = shmpcb[i].simPID;
        }
	shmdt(shmpcb);
	//printf("Killed process \n");
	exitSafe(1);
}
//function to exit safely if there is an error or anything
//it removes the IPC data.
void exitSafe(int id){
	//destroy the Semaphore memory
	if(removesem(semid) == -1) {
               perror("Error: oss: Failed to clean up semaphore");
       }
	//shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
	//fclose(fp);
        shmdt(shmpcb);
	
	//destroy msg queue
	msgctl(msgid,IPC_RMID,NULL);
	//detatch dispatch shm
	shmdt(shmpid);
	//destroy shared memory 
        shmctl(shmid,IPC_RMID,NULL);
        shmctl(shmidPCB,IPC_RMID,NULL);
	shmctl(shmidPID,IPC_RMID,NULL);
	int i;
	//kill the children.
	for(i = 0; i < 18; i++){
		if((bitMap[i/8] & (1 << (i % 8 ))) != 0){	
			//if(pidArray[i] == getpid()){
				kill(pidArray[i],SIGKILL);
			//}		
		}	
	}
	exit(0);
}

void printTable(FILE *fp);
void incrementClock(FILE *fp);
//Global for increment clock
struct sembuf semwait[1];
struct sembuf semsignal[1];


int main(int argc, char **argv){
        snprintf(bound, sizeof(bound), "%d", 250);
	//const int ALPHA = 1;
	//const int BETA = 1;
	int opt;
	//get the options from the user.
	while((opt = getopt (argc, argv, "ht:vb:")) != -1){
		switch(opt){
			case 'h':
				printf("Usage: ./oss [-t [timeout in seconds]] [-o [1 for fifo and 0 for LRU ]\n");
				exit(1);
			case 't':
				timeout = atoi(optarg);
				break;
			case 'v':
				verbose = true;
				break;
			case 'o':
                                if(optarg > 0){
					fifo = true;
				}
				else {
					fifo = false;
				}
				break;
			default:
				printf("Wrong Option used.\nUsage: ./oss [-t [timeout in seconds]] [-o [1 for fifo and 0 for LRU ]\n");
				exit(1);
		}
	}
	//set the countdown until the timeout right away.
	alarm(timeout);
        signal(SIGALRM,timerHandler);
	
	//get shared memory for clock
	key_t key = ftok("./oss",45);
	if(errno){
		perror("Error: oss: Shared Memory key not obtained: ");
		exitSafe(1);
	}	
	shmid = shmget(key,1024,0666|IPC_CREAT);
	if (shmid == -1 || errno){
		perror("Error: oss: Failed to get shared memory. ");
		exitSafe(1);
	}
	shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	shmclock->second = 0;
	shmclock->nano = 0;
	shmdt(shmclock);
 
	//get shared memory for the Process Control Table
	key_t pcbkey = ftok("./oss",'v');
	if(errno){
		perror("Error: oss: Shared Memory key not obtained: ");
                exitSafe(1);
	}
	size_t PCBSize = sizeof(struct PCB) * 18;
	shmidPCB = shmget(pcbkey,PCBSize,0666|IPC_CREAT);
        if (shmidPCB == -1 || errno){
                perror("Error: oss: Failed to get shared memory. ");
                exitSafe(1);
        }
        shmpcb = (struct PCB*) shmat(shmidPCB,(void*)0,0);
        int x;
	for(x = 0; x < 18; x++){
		shmpcb[x].simPID = 0;
		//Page table here
		int y;
		for(y=0; y<32; y++){
		
		shmpcb[x].pgTable[y].address = 0;
		shmpcb[x].pgTable[y].protection = 0;
		shmpcb[x].pgTable[y].dirty = 0;
		shmpcb[x].pgTable[y].ref = 0;
		shmpcb[x].pgTable[y].valid = 0;
		}
	}
        shmdt(shmpcb);
	//get the shared memory for the pid and quantum
	key_t pidkey = ftok("./oss",'m');
	if(errno){
                perror("Error: oss: Shared Memory key not obtained: ");
                exitSafe(1);
        }
        shmidPID = shmget(pidkey,16,0666|IPC_CREAT);
        if (shmidPID == -1 || errno){
                perror("Error: oss: Failed to get shared memory. ");
                exitSafe(1);
        }
        
	
	//Initialize semaphore
	//get the key for shared memory
	key_t semKey = ftok("/tmp",'j');
	if( semKey == (key_t) -1 || errno){
                perror("Error: oss: IPC error: ftok");
                exitSafe(1);
        }
	//get the semaphore id
	semid = semget(semKey, 1, PERMS);
       	if(semid == -1 || errno){
		perror("Error: oss: Failed to create a private semaphore");
		exitSafe(1);	
	}
	
	//declare semwait and semsignal
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	if (initElement(semid, 0, 1) == -1 || errno){
		perror("Failed to init semaphore element to 1");
		if(removesem(semid) == -1){
			perror("Failed to remove failed semaphore");
		}
		return 1;
	}

	//get message queue key
	key_t msgKey = ftok("/tmp", 'k');
	if(errno){
		
		perror("Error: oss: Could not get the key for msg queue. ");
		exitSafe(1);
	}
	//set message type
	message.mesg_type = 1;
		
	//get the msgid
	msgid = msgget(msgKey, 0600 | IPC_CREAT);
	if(msgid == -1 || errno){
		perror("Error: oss: msgid could not be obtained");
		exitSafe(2);
	}

	
//*******************************************************************
//The loop starts below here. before this there is only setup for 
//shared memory, semaphores, and message queues.
//******************************************************************	
	//open logfile
	FILE *fp;	
	remove(logFile);
	fp = fopen(logFile, "a");


	//for limit purposes.
	int lines = 0;
	processes = 0;
	int grantedRequests = 0;
	const int maxTimeBetweenNewProcsNS = 500000000;
	//const int maxTimeBetweenNewProcsSecs = 0;
	shmpid = (struct Dispatch*) shmat(shmidPID,(void*)0,0);
	struct Clock launchTime;
	launchTime.second = 0;
	launchTime.nano = 0;
	int lastPid = -1;

	//Frame Table
	struct Queue *fTable = createQueue();
	
	while(1){
		//check if we need to launch the processes
		
		//get the semaphore	
		if (r_semop(semid, semwait, 1) == -1){
                       perror("Error: oss: Failed to lock semid. ");
                       exitSafe(1);
                }
                else{	
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
			if(1){
				//if we have an open bit, fork.
				if(((lastPid = openBit(bitMap,lastPid)) != -1) && (processes < 100)){
					launchTime.second = 0;
					launchTime.nano = 0;
					//launch the process
					pid_t pid = fork();
                       			if(pid == 0){
                                		char arg[20];
                                		snprintf(arg, sizeof(arg), "%d", shmid);
                                		char spid[20];
                                		snprintf(spid, sizeof(spid), "%d", semid);
                                		char msid[20];
                                		snprintf(msid, sizeof(msid), "%d", msgid);
                                		char disId[20];
                                		snprintf(disId, sizeof(disId), "%d", shmidPID);
						char pcbID[20];
						snprintf(pcbID, sizeof(pcbID), "%d", shmidPCB);
						char bitIndex[20];
						snprintf(bitIndex, sizeof(bitIndex), "%d", lastPid);
						//char rdid[20];
						//snprintf(rdid, sizeof(rdid), "%d", shmidRD);
                                		execl("./user","./user",arg,spid,msid,disId,pcbID,bitIndex,NULL);
                                		perror("Error: oss: exec failed. ");
                                		//fclose(fp);
						exitSafe(1);
                        		}
					else if(pid == -1){
						perror("OSS: Failed to fork.");
						exitSafe(2);
					}
                        		//printf("forked child %d\n",pid);
                        		lines++;
					if(lines < 100000){
                        		fprintf(fp,"OSS: Generating process %d with pid %lld at time %d:%d\n",processes,(long long)pid,shmclock->second,shmclock->nano);
					}
					fflush(fp);	
					printf("OSS: Generating process %d with pid %lld at time %d:%d\n",processes,(long long)pid,shmclock->second,shmclock->nano);
					processes++;
                       		        //Setting up the PCB
                       		        shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
                       		        shmpcb[lastPid].simPID = pid;
					//set the bit to 1.		
					setBit(bitMap,lastPid);
                        	        shmdt(shmpcb);
				}
			}
			shmdt(shmclock);
				
			if (r_semop(semid, semsignal, 1) == -1) {
                        	perror("Error: oss: failed to signal Semaphore. ");
                               	exitSafe(1);
                	}        
		        
		}
		 //Look for message

		//if msg received, run algorithm
                if (msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT) != -1){
			if((strcmp(message.mesg_text,"Done")) == 0){
				//reset bit map
				resetBit(bitMap,message.bitIndex);
				printf("Message received Done");
				fprintf(fp,"OSS: process %lld has terminated\n",message.pid);
				int status = 0;
                                waitpid(message.pid, &status, 0);
				//deallocate the memory
				//int j;
				//for(j=0;j<32;j++){
				//	if(shmpcb[message.bitIndex].pgTable[j].valid){
				//		int addr = shmpcb[message].bitIndex.pgTable[j].address;
				//		
				//	}
			}
			else{
				//not terminateing
				if((strcmp(message.mesg_text,"Read")) == 0 || (strcmp(message.mesg_text,"Write")) == 0){
					bool write = false;
					if((strcmp(message.mesg_text,"Write")) == 0){
						write = true;
					}
					//printf("Message Received Read Request");
					message.mesg_type = message.pid;
					fprintf(fp,"OSS: process %lld has requesting page %u\n",message.pid,message.page);
					r_semop(semid, semwait, 1);
                                        bool fault = false;
					shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
					struct Node* victim;
					int vicIndex, vicFrame;
					//check to see if the page is in frame
					fprintf(fp,"size of frameTable: %d\n",sizeOfQueue(fTable));
				//	printFrameTable(fTable,fp);
					if(shmpcb[message.bitIndex].pgTable[message.page].valid != 1){
						
						//choose new frame number
						
						int newFrame = getFrameNumber(fTable);
						//check to see if the frames are all taken
						if(sizeOfQueue(fTable) == 256){
							fault = true;
							//choose a victim frame
							//int vicIndex = 0, vicFrame = 0;
							//if(fifo){
							victim = deQueue(fTable);
							enQueue(fTable,message.bitIndex,newFrame);
							shmpcb[message.bitIndex].pgTable[message.page].valid = 1;
                                                        shmpcb[message.bitIndex].pgTable[message.page].address = newFrame;

							//}
							//else{
							// LRU
								
							//}
							vicIndex = victim->key;
							vicFrame = victim->frame;
						}
						else{
							enQueue(fTable,message.bitIndex,newFrame);
							shmpcb[message.bitIndex].pgTable[message.page].valid = 1;
                                                        shmpcb[message.bitIndex].pgTable[message.page].address = newFrame;
							//incrementClock(fp);
						}
					}
					else{
						fprintf(fp,"OSS: page found in frame %d",shmpcb[message.bitIndex].pgTable[message.page].address);
						//page is in frame already
						if(!fifo){
							struct Node* n = deQueue(fTable);
							enQueue(fTable,n->key,n->frame);
						}
						//incrementClock(fp);
					}
					if(fault){
						//handle victim
						int j;
						for(j=0;j<32;j++){
							if(shmpcb[vicIndex].pgTable[j].address == vicFrame && shmpcb[vicIndex].pgTable[j].valid == true){ 
								shmpcb[vicIndex].pgTable[j].valid = false;
								fprintf(fp,"OSS: Page Fault! victim frame #%d in process %lld's page #%d\n",vicFrame,shmpcb[vicIndex].simPID,j);
								printf("OSS: Page Fault! victim frame #%d in process %lld's page #%d\n",vicFrame,shmpcb[vicIndex].simPID,j);
								break;
							}
						}
					}
					shmdt(shmpcb);
					r_semop(semid,semsignal,1);
					
					message.pid = message.pid;
					if(fault){
						sprintf(message.mesg_text,"Page Fault");
						
					}
					else{
						sprintf(message.mesg_text,"No Fault");
					}
					msgsnd(msgid, &message, sizeof(message),0);
						
				}

			}
				
		}else{
			errno = 0;
		//	printf("No message Received increment clock and loop back\n");
		}
		r_semop(semid,semsignal,1);
		//if not then just increment clock and loop back
		incrementClock(fp);	
	}

	return 0;
}

//print the current frame table;
void printFrameTable(const struct Queue* q,FILE* fp){
	struct Node* node = q->front;
	if(node == NULL){
		return;
	}
	fprintf(fp,"OSS: frame table: <%d,",node->frame);
	while(node->next != NULL){
		node = node->next;
		fprintf(fp,"%d,",node->frame);
	}
	fprintf(fp,">\n");
}

//Increment the clock after each iteration of the loop.
//by 1.xx seconds with xx nanoseconds between 1-1000 and 5MS
void incrementClock(FILE * fp){
		if (r_semop(semid, semwait, 1) == -1){
                        perror("Error: oss: Failed to lock semid. ");
                        exitSafe(1);
                }
                else{
			/*make sure we convert the nanoseconds to seconds after it gets large enough*/
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
                        unsigned int increment = 100;//(unsigned int)rand() % 1000 + 5000000;
                        if(shmclock->nano >= 1000000000 - increment){
                                shmclock->second += (unsigned int)(shmclock->nano / (unsigned int)1000000000);
                                shmclock->nano = (unsigned int)(shmclock->nano % (unsigned int)1000000000) + increment;
                        }
                        else{
                                shmclock->nano += increment;
                        }
			//if(errno){
			//	perror("ERROR SOMETHING:");
			//	exitSafe(1);
			//}
			if(currentSecond != shmclock->second){
				currentSecond = shmclock->second;
				fprintf(fp,"OSS: Current Time: %d:%d\n",shmclock->second,shmclock->nano);
			}

			//printf("Current Time: %d:%d\n",shmclock->second,shmclock->nano);
                        shmdt(shmclock);
                        if (r_semop(semid, semsignal, 1) == -1) {
                                perror("Error: oss: failed to signal Semaphore in incrementClock. ");
                                exitSafe(1);
                        }
               }
}
