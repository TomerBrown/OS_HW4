#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <pthread.h>

#define SUCCESFULL 0
#define PROBELM -1




//-------------------------------------------------------------------------
//                         Queue Related Functions
//-------------------------------------------------------------------------

/*A struct to represent a node in the queue
Fields:
1) void* value (Will be char* for directory queue and int* for thread queue)
2) Node* next
*/
typedef struct Node {
    struct Node* next;
    void* value;
} Node;

/*A function that initializes a node in the queue with dir_name value*/
Node* init_Node (char* dir_name){
    Node* node = calloc(1 , sizeof(Node));
    node -> next = NULL;
    node ->value = (void*) dir_name;
    return node;
}

/*A struct to represent a Queue
Fields:
1) int len
2) Node* head
3) Node* tail
*/
typedef struct Queue {
    int len;
    Node* head;
    Node* tail;
} Queue;

/*A function that initializes a empty queue*/
Queue init_Queue(){
    Queue* queue = calloc(1,sizeof(Queue));
    queue->len = 0;
    queue->head = NULL;
    queue->tail = NULL;
    return *queue;
}

/*Returns 1 if and only if the queue is empty (e.i. it's length is 0) */  
int is_empty(Queue* q){
    return ((q->len)==0);
    //todo: Add locks to avoid collisions (Changes in queue in the middle of is_empty)
}

/*given a dir_name to enter to queue, the function inserts it as the last element and changes pointers accordingly*/
void insert (Queue* q ,char* dir_name){
    Node* node = init_Node(dir_name);
    if (q->len == 0){
        q->head = node;
        q->tail = node;
    }
    else{
        q->tail->next = node;
        q->tail = node;
    }
    q->len ++;
    //todo: Lock neccessary parts to avoid collisions
}

/*Returns the first dir_name in the queue (by FIFO order) and removes it from queue*/
char* pull(Queue* q ){
    
    //assert it is not empty for debugging purposes
    assert(!is_empty(q));

    //git the first one
    Node* node = q->head;
    
    //copy the dir_name content
    char* dir_name = malloc(strlen(node->value));
    strcpy(dir_name,node->value);

    //remove the first element and free it
    q->head = node->next;
    q->len--;
    free(node);
    return dir_name;
}

/*Iterate throgh the queue and print it element by element*/
void print_queue(Queue* q){
    int l = q->len;
    Node* node = q->head;
    printf ("[");
    for (int i=0;i<l;i++){
        if (i!= l-1){
            printf("%s,",(char*)node->value);
        }
        else{
            printf("%s",(char*) node->value);
        }
        
        node = node->next;
    }
    printf("]\n");
}

void free_queue(Queue* q){
    int len = q->len;
    if (len ==0){
        return;
    }
    Node* node = q->head;
    Node* next = q->head->next;
    for (int i=0; i<len-1; i++){
        free(node);
        node = next;
        next = node->next;
    }
}

/*Given a message to print the function prints it and then frees the message*/
void print_message (char* message){
    printf("%s\n",message);
    free(message);
}

void basic_queue_test(){
    Queue q = init_Queue();
    printf("Is empty : %s (should be true)\n",is_empty(&q)? "true" : "false" );
    insert(&q, "Tomer");
    insert(&q, "Efrat");
    insert(&q, "Niv");
    insert(&q, "Hava");
    insert(&q, "David");
    print_queue(&q);
    printf("Is empty : %s (should be false)\n",is_empty(&q)? "true" : "false" );
    printf("Len is: %d (should be 5)\n",q.len);
    print_message(pull(&q));
    print_message(pull(&q));
    print_message(pull(&q));
    printf("Len is: %d (should be 2)\n",q.len);
    print_message(pull(&q));
    print_message(pull(&q));
    printf("Len is: %d (should be 0)\n",q.len);
    insert(&q, "Tomer");
    insert(&q, "Efrat");
    insert(&q, "Niv");
    insert(&q, "Hava");
    insert(&q, "David");
    print_queue(&q);
    free_queue(&q);
}


/* -----------------------------------------------------------------------------------------------
                                 Parsing the input  
-------------------------------------------------------------------------------------------------*/

/*Given a string s checks if contains only digits and then converts it to integer
    if the string contains non-digit charachters , the function returns -1.
    otherwise, the function returns an integer representing the converted number.
    */
int string_to_num(char* s){
    
    int i;
    char c;
    int l = strlen(s);
    for (i=0; i<l ;i++){
        c = s[i];
        if (c<'0' || c >'9'){
            return -1;
        }
    }
    return atoi(s);
}


/* -----------------------------------------------------------------------------------------------
-----------------------------    Directory search functions   ------------------------------------
-------------------------------------------------------------------------------------------------*/

/*The function returns 1 if and only if term is contained in name and 0 otherwise*/
int name_contains_term(char* name, const char* term){
    return strstr(name,term)!=NULL;
}

char* extend_path (char* original_path,char* extention){
    char* exteneded_path = calloc(1,PATH_MAX);
    sprintf(exteneded_path,"%s/%s",original_path,extention);
    return exteneded_path;
}



/* -----------------------------------------------------------------------------------------------
-----------------------------------------    Global Variables -----------------------------------
-------------------------------------------------------------------------------------------------*/

char* term;
int n;

pthread_mutex_t all_initialized_lock;
atomic_int initialized_counter = 0;
pthread_cond_t all_initialized_cond;
Queue directory_queue;

Queue threads_queue;

/* -----------------------------------------------------------------------------------------------
-----------------------------------------    Thread Code -----------------------------------------
-------------------------------------------------------------------------------------------------*/

void* searching_thread (void* arg){
    int tid = *(int*)arg;
    printf("%d: started\n",tid);
    pthread_mutex_lock(&all_initialized_lock);
    initialized_counter++;
    while (initialized_counter < n){
        printf("%d: waiting\n",tid);
        pthread_cond_wait(&all_initialized_cond, &all_initialized_lock);
        printf("%d: awaken\n",tid);
    }
    pthread_cond_broadcast(&all_initialized_cond);
    pthread_mutex_unlock(&all_initialized_lock);
    
    return arg;
}

/* -----------------------------------------------------------------------------------------------
-------------------------------------------    Main   --------------------------------------------
-------------------------------------------------------------------------------------------------*/

int main(int argc, char* argv[]){

    //Firstly: Parse and check argument given from the command line
    if (argc != 4) {
        fprintf(stderr,"Error: Too many or not enough arguments given to program. please check and retry.\n");
        return 1;
    }

    char* root_directory = argv[1];
    term = argv[2];
    n = string_to_num(argv[3]); //num of threads to create

    if (n <= 0){
        fprintf(stderr, "Error: number of threads entered (i.e third argument) is invalid.\n");
        return 1;
    }

    //Initialize all locks and condition Variables
    pthread_mutex_init(&all_initialized_lock, NULL);
    pthread_cond_init (&all_initialized_cond,NULL);
    
    //1. Create a FIFO queue that holds directories.
    directory_queue = init_Queue();
    threads_queue = init_Queue();

    //2. Put the search root directory (where to start the search, specified in argv[1]) in the queue
    insert(&directory_queue, root_directory);

    int* tid;
    //3. Create n searching threads (as per the number received in argv[3]).
    pthread_t threads[n];
    for (int i=0; i<n ;i++){
        tid = malloc(sizeof(int));
        *tid = i;
        pthread_create(&threads[i],NULL,&searching_thread, (void*)tid);
    }

    //4.After all searching threads are created, the main thread signals them to start searching.
    //printf("Main: Broadcast all initialized, counter is: %d\n",initialized_counter);
    

    for (int i=0; i<n ;i++){
        pthread_join(threads[i], NULL);
    }

    printf("root directory : '%s'  | term: %s | n: %d\n",root_directory,term,n);
    //print_queue(&directory_queue);

    free_queue(&directory_queue);

    //Destroy all locks and condition variables
    pthread_mutex_destroy(&all_initialized_lock);
    pthread_cond_destroy(&all_initialized_cond);
    return 0;
}
