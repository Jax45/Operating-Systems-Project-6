//structure for the dispatch of quantum and pid
struct Dispatch{
	int index;
        int quantum;
        long long int pid;
	int flag;
};

//structure for the shared memory clock
struct Clock{
        unsigned int second;
        unsigned int nano;
};

//page table
struct PageTable{
	unsigned int address :8;
	unsigned int protection :1;	
	unsigned int dirty :1;
	unsigned int ref :1;
	unsigned int valid :1;
};



//Structure for the Process Control Block
struct PCB{
        pid_t simPID;
	struct PageTable pgTable[32];
};
//Struct for the Resource Descriptor
struct RD{
	bool sharable;
	short total;
	short available;
};
//for the message queue
struct mesg_buffer {
        long mesg_type;
	pid_t pid;
	int bitIndex;
	unsigned int page;
        char mesg_text[100];
} message;

//for the priority queue's
//Node in a queue
struct Node {
	int key; //frame
	int frame; //index
	struct Node* next;
};
//Queue
struct Queue {
	struct Node *front, *rear;
};
//node for queue
struct Node* newNode(int key, int frame){
	struct Node *temp = (struct Node*)malloc(sizeof(struct Node));
	temp->key = key;
	temp->frame = frame;
	temp->next = NULL;
	return temp;
}

struct Queue* createQueue(){
	struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue)); 
        q->front = q->rear = NULL; 
        return q; 
} 

void enQueue(struct Queue* q, int key, int frame) 
{ 
	struct Node* temp = newNode(key,frame); 
	if (q->rear == NULL) { 
	        q->front = q->rear = temp; 
	        return; 
	} 
	q->rear->next = temp; 
	q->rear = temp; 
}
struct Node* deQueue(struct Queue* q) { 
	if (q->front == NULL){ 
		return NULL; 
	}
	struct Node* temp = q->front; 
	//free(temp); 
	//int temp = q->front->key;
	q->front = q->front->next; 	
	if (q->front == NULL) {
	        q->rear = NULL;
	} 
	return temp; 
}
//get a unique frame number not being used currently
//and return it.
int getFrameNumber(const struct Queue* q){
	int i = 0;
	struct Node * n;
	for(i=0;i<256;i++){
		//check that i is not being used;
		bool isUsed = false;
		n = q->front;
		while(n != NULL){
			if(i == n->frame){
				isUsed = true;
				break;
			}
			n = n->next;
		}
		if(!isUsed){
			return i;
		}
	}
	return i;
}

//get the size of the queue without dequeueing
int sizeOfQueue(const struct Queue* q) {
	if(q->front == NULL){
		return 0;
	}
	int i = 1;
	struct Node *n = q->front->next;
	while(n != NULL){
		i++;
		n = n->next;
	}
	return i;
}
