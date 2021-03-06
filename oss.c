/************************************************************************************
 * Name: Jackson Hoenig
 * Class: CMPSCI 4760
 * Project 5
 * Description:
 *In this program, the Operating system simulator starts by allocating shared memory
for the Process Control block that has access to a page table of size 32k with each page being
1k large. the total size of the operating system has access to is assumed to be 256k. which is kept track
of as frames in a fTable (frame table) of size 256. at random times from 1-500 nanoseconds a process is forked.
then that process can request for a read or write of a page. the OSS receives this message and checks to see if
it is memory of the frame table. if it is then it is given access and 1 is set for the dirty bit if it is a write.
if it is not in the table then a pagefault occurs and the process waits in a queue until 14ms of logical time have passed.
if all of the max processes are full then the clock is sped up to the first waiting process.
every 1 second the allocation of the frame table is shown. Every 1000 memory accesses a process has the chance to terminate
if it does randomly choose to terminate then the memory is deallocated and the page table and frame table are updated to see that.


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
#define PROC_LIM 18
//global var.
int memAccess = 0;
int pgFaults = 0;
int processes = 0;
char bound[20];
bool verbose = false;
int currentSecond = 0;
double currentTime = 0;
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
int maxProcesses = 18;

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
	FILE * fp;
	if(fifo){
		fp = fopen("fifolog.txt", "w");
	}
	else{
		fp = fopen("lrulog.txt", "w");
	}
	fprintf(fp,"Number of processes: %d\n",processes);
        fprintf(fp,"Number of memory accesses: %d\n",memAccess);
        fprintf(fp,"Number of page faults: %d\n",pgFaults);
        fprintf(fp,"Simulated time: %f\n",currentTime);
        fprintf(fp,"Memory access per second: %f\n",(double)memAccess/currentTime);
        fprintf(fp,"Page fault per memory access: %f\n",(double)pgFaults/memAccess);
	fclose(fp);

	printf("Number of processes: %d\n",processes);
	printf("Number of memory accesses: %d\n",memAccess);
	printf("Number of page faults: %d\n",pgFaults);
	printf("Simulated time: %f\n",currentTime);
	printf("Memory access per second: %f\n",(double)memAccess/currentTime);
	printf("Page fault per memory access: %f\n",(double)pgFaults/memAccess);
	if(fifo){
		printf("\nData above stored in fifolog.txt\n");
	}
	else{
                printf("\nData above stored in lrulog.txt\n");
	}
	printf("Extra log data in \"log.txt\"\n");
	//average
	//total
	fflush(stdout);
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
void incrementClock(FILE *fp,const struct Queue* q);
//Global for increment clock
struct sembuf semwait[1];
struct sembuf semsignal[1];


int main(int argc, char **argv){
	//queue to keep track of pending 14ms requests
        struct Queue *pending = createQueue();
	snprintf(bound, sizeof(bound), "%d", 250);
	//const int ALPHA = 1;
	//const int BETA = 1;
	int opt;
	//get the options from the user.
	while((opt = getopt (argc, argv, "ht:o:p:")) != -1){
		switch(opt){
			case 'h':
				printf("Usage: ./oss [-t [timeout in seconds]] [-o [1 for fifo and 0 for LRU ]\n");
				exit(1);
			case 't':
				timeout = atoi(optarg);
				break;
			case 'o':
                                if(atoi(optarg) > 0){
					fifo = true;
				}
				else {
					fifo = false;
				}
				break;
			case 'p':
				//int limit = atoi(optarg);
				if(atoi(optarg) <= PROC_LIM && atoi(optarg) > 0){
					maxProcesses = atoi(optarg);
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
	
	//prints
	if(fifo){
		printf("Simulating the First in first out (FIFO) method\n");
	}
	else{
                printf("Simulating the Least Recently Used (LRU) method\n");

	}
	
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
	shmpid = (struct Dispatch*) shmat(shmidPID,(void*)0,0);
	struct Clock launchTime;
	launchTime.second = 0;
	launchTime.nano = 0;
	int lastPid = -1;

	//Frame Table
	struct Queue *fTable = createQueue();
	

	//key is bit index
	//frame is not needed
	
	while(1){
		//check if we need to launch the processes
		if(launchTime.second == 0 && launchTime.nano == 0){


			fprintf(fp,"OSS: generating a new launch time.\n");
			r_semop(semid,semwait,1);
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
			launchTime.second = shmclock->second;
			launchTime.nano = shmclock->nano;
			unsigned int increment = rand() % 500 + 1;
			/*time between 1 - 500 ns from now*/
			if(launchTime.nano >= 1000000000 - increment){
				launchTime.second += launchTime.nano / 1000000000;
				launchTime.nano += launchTime.nano % 1000000000;
			}
			else{
				launchTime.nano += increment;
			}
			shmdt(shmclock);
			r_semop(semid,semsignal,1);
		}
	
		//get the semaphore	
		if (r_semop(semid, semwait, 1) == -1){
                       perror("Error: oss: Failed to lock semid. ");
                       exitSafe(1);
                }
                else{	
			//check launch time
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
			if(shmclock->second > launchTime.second || (shmclock->second == launchTime.second && shmclock->nano > launchTime.nano)){
				//if we have an open bit, fork.
				if(((lastPid = openBit(bitMap,lastPid,maxProcesses)) != -1) && (processes < 100)){
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
					//printf("OSS: Generating process %d with pid %lld at time %d:%d\n",processes,(long long)pid,shmclock->second,shmclock->nano);
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
				//printf("Message received Done");
				fprintf(fp,"OSS: process %lld has terminated Releasing memory\n",(long long)message.pid);
				printf("OSS: process %lld has terminated Releasing memory\n",(long long)message.pid);
				int status = 0;
                                waitpid(message.pid, &status, 0);
				//deallocate the memory
				int j;
				r_semop(semid,semwait,1);
                                shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);

				for(j=0;j<32;j++){
					if(shmpcb[message.bitIndex].pgTable[j].valid == 1){
						int addr = shmpcb[message.bitIndex].pgTable[j].address;
						shmpcb[message.bitIndex].pgTable[j].address = 0;
						shmpcb[message.bitIndex].pgTable[j].valid = 0;
						shmpcb[message.bitIndex].pgTable[j].dirty = 0;
						struct Node* n = fTable->front;
						while(n != NULL){
							if(n->key == message.bitIndex && addr == n->frame){
								n->key = -1;
							}
							n = n->next;
						}	
					}
				}
				shmdt(shmpcb);
				r_semop(semid,semsignal,1);
			}
			else{
				//not terminateing
				if((strcmp(message.mesg_text,"Read")) == 0 || (strcmp(message.mesg_text,"Write")) == 0){
					int address = message.page;
					message.page = message.page >> 10;
					fprintf(fp,"Address received: %d, Page: %d\n",address,message.page);	
					bool write = false;
					memAccess++;
					if((strcmp(message.mesg_text,"Write")) == 0){
						write = true;
					}
					//printf("Message Received Read Request");
					message.mesg_type = message.pid;
					fprintf(fp,"OSS: process %lld has requesting page %u",(long long)message.pid,message.page);
					if(write){
						fprintf(fp, " for a Write operation.\n");
					}
					else{
						fprintf(fp, " for a Read operation.\n");
					}
					r_semop(semid, semwait, 1);
                                        bool fault = false;
                                        shmclock = (struct Clock*) shmat(shmid, (void*)0,0);

					shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
					struct Node* victim;
					int vicIndex, vicFrame;
					//check to see if the page is in frame
					if(shmpcb[message.bitIndex].pgTable[message.page].valid != 1){
						
						//choose new frame number
						
						int newFrame = getFrameNumber(fTable);
						//check to see if the frames are all taken
						if(sizeOfQueue(fTable) == 256){
							//shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
							//get current time
							shmpcb[message.bitIndex].waitNano = shmclock->nano;
                                                        shmpcb[message.bitIndex].waitSecond = shmclock->second;

							unsigned int incr = 14000000;
							if(shmclock->nano >= 1000000000 - incr){
								shmpcb[message.bitIndex].waitSecond += shmpcb[message.bitIndex].waitNano / 1000000000;
								shmpcb[message.bitIndex].waitNano += shmpcb[message.bitIndex].waitNano % 1000000000;
							}
							else{
								shmpcb[message.bitIndex].waitNano = shmclock->nano + incr;
								shmpcb[message.bitIndex].waitSecond = shmclock->second;
							}
							fault = true;
							pgFaults++;
							//choose a victim frame
							//int vicIndex = 0, vicFrame = 0;
							//if(fifo){
							victim = deQueue(fTable);
							//make sure we have a valid victim.
							while(victim->key == -1){
								enQueue(fTable,victim->key,victim->frame);
								victim = deQueue(fTable);
							}
							enQueue(fTable,message.bitIndex,newFrame);
							shmpcb[message.bitIndex].pgTable[message.page].valid = 1;
							if(write){
                                                        	shmpcb[message.bitIndex].pgTable[message.page].dirty = 1;
                                                        }
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
                                                        if(write){
                                                                shmpcb[message.bitIndex].pgTable[message.page].dirty = 1;
                                                        }

							shmpcb[message.bitIndex].pgTable[message.page].address = newFrame;
							
							//still a page fault
							fprintf(fp,"OSS: Page Fault! address %d not in frame\n",address);
							//printf("OSS: Page Fault! address %d not in frame\n",address);	
							pgFaults++;
						}
					}
					else{
						if(!write){
						fprintf(fp,"OSS: address %d is in frame %d giving data to Process %d at time %d:%d\n",address,shmpcb[message.bitIndex].pgTable[message.page].address,shmpcb[message.bitIndex].simPID,shmclock->second,shmclock->nano);
						}else{
						fprintf(fp,"OSS: address %d is in frame %d writing data to Process %d at time %d:%d\n",address,shmpcb[message.bitIndex].pgTable[message.page].address,shmpcb[message.bitIndex].simPID,shmclock->second,shmclock->nano);
						}
						//page is in frame already
						
						//if dirty bit make sure to add 10ms to clock
						//otherwise make it 10 ns.
						unsigned int increment = 10;
						
						if(shmpcb[message.bitIndex].pgTable[message.page].dirty == 1){
							increment = 10000000;
                        			}
	
                       				if(shmclock->nano >= 1000000000 - increment){
                       				        shmclock->second += shmclock->nano / 1000000000;
                       				        shmclock->nano += shmclock->nano % 1000000000;
                       				}
                       				else{
		                               		shmclock->nano += increment;
                        			}
						
						//if LRU dequeeue and enqueue to make it a reference.
						if(!fifo){
							struct Node* n = deQueue(fTable);
							enQueue(fTable,n->key,n->frame);
						}
					}
					if(fault){
						//handle victim
						int j;
						for(j=0;j<32;j++){
							if(shmpcb[vicIndex].pgTable[j].address == vicFrame && shmpcb[vicIndex].pgTable[j].valid == 1){ 
								shmpcb[vicIndex].pgTable[j].valid = 0;
								fprintf(fp,"OSS: Page Fault! victim frame #%d in process %lld's page #%d\n",vicFrame,(long long)shmpcb[vicIndex].simPID,j);
								//printf("OSS: Page Fault! victim frame #%d in process %lld's page #%d\n",vicFrame,shmpcb[vicIndex].simPID,j);
								break;
							}
						}
					}
					shmdt(shmclock);
					shmdt(shmpcb);
					r_semop(semid,semsignal,1);
					
					message.pid = message.pid;
					if(fault){
						//sprintf(message.mesg_text,"Page Fault");
						enQueue(pending,message.bitIndex,0);		
					}
					else{
						sprintf(message.mesg_text,"No Fault");
						msgsnd(msgid, &message, sizeof(message),0);
					}
				}

			}
				
		}else{
			errno = 0;
		//	printf("No message Received increment clock and loop back\n");
		}
		

		r_semop(semid,semwait,1);
                shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
                shmpcb = (struct PCB*) shmat(shmidPCB, (void*)0,0);
                struct Node * n = pending->front;
		int size = sizeOfQueue(pending);
		if(n != NULL){
			//if the size is 18 speed up to prevent deadlock
			//speed up to first in queue.
			int sec = shmpcb[n->key].waitSecond;
	                int nano = shmpcb[n->key].waitNano;
	
			if(size >= 17){
                		shmclock->second = sec;
                	        shmclock->nano = nano + 10;
                	}
		}
		int i;
		//Check the pending pagefaults
		for(i=0;i<size;i++){
	//		struct Node * n = pending->front;
			if(n != NULL){
				//check to see if time passed
				unsigned int sec = shmpcb[n->key].waitSecond;
				unsigned int nano = shmpcb[n->key].waitNano;
				if((sec == shmclock->second && nano < shmclock->nano) || sec < shmclock->second){
					//send message
					message.pid = shmpcb[n->key].simPID;
					message.mesg_type = message.pid;
					sprintf(message.mesg_text,"Page Fault");
                                        msgsnd(msgid, &message, sizeof(message),0);
	
					//dequeue
					deQueue(pending);
					n = n->next;	
				}
				else{
					//Time has not passed yet check next one
					n = n->next;	
				}
			}
			else{
				// n is null
				break;
			}
		}
		shmdt(shmclock);
                shmdt(shmpcb);
                
                r_semop(semid,semsignal,1);




		//if not then just increment clock and loop back
		incrementClock(fp,fTable);	
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
		if(node->frame == -1){
	                fprintf(fp,".,");

		}
		fprintf(fp,"%d,",node->frame);
	}
	fprintf(fp,">\n");
}

//Increment the clock after each iteration of the loop.
//by 1.xx seconds with xx nanoseconds between 1-1000 and 5MS
void incrementClock(FILE * fp,const struct Queue* q){
		if (r_semop(semid, semwait, 1) == -1){
                        perror("Error: oss: Failed to lock semid. ");
                        exitSafe(1);
                }
                else{
			/*make sure we convert the nanoseconds to seconds after it gets large enough*/
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
                        unsigned int increment = 10;//(unsigned int)rand() % 1000 + 5000000;
                        if(shmclock->nano >= 1000000000 - increment){
                                shmclock->second += (unsigned int)(shmclock->nano / (unsigned int)1000000000);
                                shmclock->nano = (unsigned int)(shmclock->nano % (unsigned int)1000000000) + increment;
                        }
                        else{
                                shmclock->nano += increment;
                        }
			if(currentSecond != shmclock->second){
				currentSecond = shmclock->second;
				fprintf(fp,"OSS: Current Time: %d:%d\n",shmclock->second,shmclock->nano);
				printFrameTable(q,fp);
			}
			currentTime = (double)shmclock->second + ((double)shmclock->nano / 1000000000.0);
			//printf("Current Time: %d:%d\n",shmclock->second,shmclock->nano);
                        shmdt(shmclock);
                        if (r_semop(semid, semsignal, 1) == -1) {
                                perror("Error: oss: failed to signal Semaphore in incrementClock. ");
                                exitSafe(1);
                        }
               }
}
