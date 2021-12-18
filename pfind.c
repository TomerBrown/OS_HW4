// By : Tomer Brown
// ID: 318338415


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


// ---------------------------------------------------------------------------------------
//                           General Implemantation Details
// ---------------------------------------------------------------------------------------
/*
Two Queue are maintaind:
1) directory_queue : for directories to be checked by order of insertion FIFO
2) threads_queue : for making sure order of awaiking is maintained


1) All thread will begin only after all are initialized ==> ** all_initialized_cond, all_initialized_lock ** - are for that
2) All queue operation will be done atomicly is ensure by **queue_lock**
3) Order of awaking from sleep is done by **first_out_cond** (To ensure it)
4) To wait and until the queue is non empty - **empty_cond** is being used
5) In order to assure Correct operation of the thread a critical part of checking what a thread  need to do  => **thread_lock**
*/



// ---------------------------------------------------------------------------------------
//                           Struct and Types use by data structure
// ---------------------------------------------------------------------------------------

/*A struct to represent a node in the queue
Fields:
1) void* value (Will be char* for directory queue and int* for thread queue)
2) Node* next
*/
typedef struct Node {
    struct Node* next;
    void* value;
} Node;

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

/* -----------------------------------------------------------------------------------------------
-----------------------------------------    Global Variables -----------------------------------
-------------------------------------------------------------------------------------------------*/

char* term;
int n;

pthread_mutex_t all_initialized_lock;
atomic_int initialized_counter = 0;
pthread_cond_t all_initialized_cond;


atomic_int error_threads = 0;
atomic_int count_found = 0;

pthread_mutex_t queue_lock;
pthread_mutex_t thread_lock;
pthread_cond_t empty_cond;
pthread_cond_t first_out_cond;

Queue directory_queue;
Queue threads_queue;



//-------------------------------------------------------------------------
//                         Queue Related Functions
//-------------------------------------------------------------------------



/*A function that initializes a node in the queue with dir_name value*/
Node* init_Node (void* value){
    Node* node = calloc(1 , sizeof(Node));
    if (node == NULL){
        return NULL;
    }
    node -> next = NULL;
    node ->value = value;
    return node;
}


/*A function that initializes a empty queue*/
Queue* init_Queue(){
    Queue* queue = calloc(1,sizeof(Queue));
    if (queue==NULL){
        return NULL;
    }
    queue->len = 0;
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

/*Returns 1 if and only if the queue is empty (e.i. it's length is 0) */  
int is_empty(Queue* q){
    pthread_mutex_lock(&queue_lock);
    int x = (q->len)==0;
    pthread_mutex_unlock(&queue_lock);
    return x;
    //todo: Add locks to avoid collisions (Changes in queue in the middle of is_empty)
}


/*given a dir_name to enter to queue, the function inserts it as the last element and changes pointers accordingly*/
int insert (Queue* q ,void* value){
    Node* node = init_Node(value);
    if (node == NULL){
        return PROBELM;
    }
    pthread_mutex_lock(&queue_lock);
    if (q->len == 0){
        q->head = node;
        q->tail = node;
    }
    else{
        q->tail->next = node;
        q->tail = node;
    }
    q->len ++;
    pthread_mutex_unlock(&queue_lock);
    return SUCCESFULL;
}


/*Returns the first dir_name in the queue (by FIFO order) and removes it from queue*/
Node* pull (Queue* q ){
    pthread_mutex_lock(&queue_lock);
    //git the first one
    Node* node = q->head;
    //remove the first element and free it
    q->head = node->next;
    q->len--;
    pthread_mutex_unlock(&queue_lock);
    return node;
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


/*A function to free the queue*/
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

/*A function that given a path returns an extension of it with slashes*/
char* extend_path (char* original_path,char* extention){
    char* exteneded_path = calloc(1,PATH_MAX);
    sprintf(exteneded_path,"%s/%s",original_path,extention);
    return exteneded_path;
}


/* -----------------------------------------------------------------------------------------------
-----------------------------------------    Thread Code -----------------------------------------
-------------------------------------------------------------------------------------------------*/

/*This function performs the search inside a directory that was dequeued from directory_queue
when it inserts a new directory to queue it broadcasts the information so other threads will become active
*/
int search_directory(char* dir_name){
    struct stat info; // A struct to hold some data about file (linux fs api)
    char* exteneded_path; // A string to hold the concatenated path
    struct dirent* x; //could be either a file or a folder
    DIR* dir = opendir(dir_name);
    if (dir==NULL){
        printf("Directory %s: Permission denied.\n", dir_name);
        return PROBELM;
    }
    
    x = readdir(dir);
    while (x!=NULL){
        if (strcmp(x->d_name,".")==0 || strcmp(x->d_name,"..")==0 ){
            //case it is . or .. ignore it
            x = readdir(dir);
            continue;
        }
        exteneded_path = extend_path(dir_name, x->d_name);
        if (lstat(exteneded_path,&info) < 0){
            fprintf(stderr,"Error: can't get stat of directory entry for %s\n",exteneded_path);
            return PROBELM;
        }
        
        if (S_ISDIR(info.st_mode)){
            if (!(info.st_mode & S_IRUSR) && (info.st_mode & S_IXUSR)){
                printf("Directory %s: Permission denied.\n", exteneded_path);
                x = readdir(dir);
                continue;
            }
            else if (insert(&directory_queue , exteneded_path)==PROBELM){
                return PROBELM;
            }
            else{
                pthread_cond_broadcast(&empty_cond);
                pthread_cond_broadcast(&first_out_cond);
            }
            
            
        }
        else if (name_contains_term(x->d_name,term)){
            //if the filename contains the term -> print it and increment the counterS
            printf("%s\n",exteneded_path);
            count_found ++;
            free (exteneded_path);
        }
        x = readdir(dir);
    }

    return 0;
}

/*This Function is the function that is being runned from main function.
Some locks and Condition Variables are used here to ensure */
void* searching_thread (void* arg){
    int tid = *(int*)arg;
    Node* node;
    char* dir_name = malloc(PATH_MAX);
    pthread_mutex_lock(&all_initialized_lock);
    initialized_counter++;
    while (initialized_counter < n){
        pthread_cond_wait(&all_initialized_cond, &all_initialized_lock);
    }
    pthread_cond_broadcast(&all_initialized_cond);
    pthread_mutex_unlock(&all_initialized_lock);

    int waiting_flag = 0;
    while (1){
        
        pthread_mutex_lock(&thread_lock);
        while (is_empty(&directory_queue)){
            
            if (waiting_flag == 0 ){
                // If it is the first time it waits for the queue to get full. mark it and insert it to queue.
                waiting_flag = 1;
                 if (insert(&threads_queue, arg)==PROBELM){
                    error_threads++;
                    pthread_mutex_unlock(&thread_lock);
                    pthread_cond_broadcast(&empty_cond);
                    pthread_cond_broadcast(&first_out_cond);
                    pthread_exit(NULL);
                 }
                pthread_cond_broadcast(&empty_cond);
                pthread_cond_broadcast(&first_out_cond);
            }
                
                
            if (threads_queue.len + error_threads == n && is_empty(&directory_queue)){
                //If it is the end - tell everyone it is the end and finish
                pthread_mutex_unlock(&thread_lock);
                pthread_cond_broadcast(&empty_cond);
                pthread_cond_broadcast(&first_out_cond);
                return SUCCESFULL;
            }
            
            pthread_cond_wait(&empty_cond, &thread_lock);
            while (*(int*)threads_queue.head ->value != tid){
                pthread_cond_wait(&first_out_cond,&thread_lock);
            }
            if (waiting_flag == 1){
                waiting_flag = 0;
                pull(&threads_queue);
            }
        }
        //Now we know that the thread has job to do - and that it has the lock
        node = pull(&directory_queue);
        pthread_mutex_unlock(&thread_lock);
        pthread_cond_broadcast(&empty_cond);
        pthread_cond_broadcast(&first_out_cond);
        strcpy(dir_name,(char*) node->value);
        free(node);
        if (search_directory(dir_name)== PROBELM){
            error_threads++;
            pthread_cond_broadcast(&empty_cond);
            pthread_cond_broadcast(&first_out_cond);
            pthread_exit(NULL);
        }
        
    }
    return arg;
}

/* -----------------------------------------------------------------------------------------------
-------------------------------------------    Main   --------------------------------------------
-------------------------------------------------------------------------------------------------*/

/*This function frees and destroys everything it needs and return the 0 or 1 depending on error_code*/
int finalize(int error_code){
    
    free_queue(&directory_queue);
    free_queue(&threads_queue);

    //Destroy all locks and condition variables
    pthread_mutex_destroy(&all_initialized_lock);
    pthread_cond_destroy(&all_initialized_cond);

    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&thread_lock);
    pthread_cond_destroy(&empty_cond);
    pthread_cond_destroy(&first_out_cond);
    return error_code;
}


int main(int argc, char* argv[]){

    struct stat info;
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

    if (lstat(root_directory,&info) < 0){
            fprintf(stderr,"Error: can't get stat of directory entry for (main) %s\n",root_directory);
            return 1;
        }
    if (!((info.st_mode & S_IRUSR) && (info.st_mode & S_IXUSR))){
        printf("Directory %s: Permission denied.\n", root_directory);
        return 1;
    }

    //1. Create a FIFO queue that holds directories.
    Queue* ptr1;
    Queue* ptr2;
    ptr1 = init_Queue();
    ptr2 = init_Queue(); 
    if (ptr1==NULL || ptr2==NULL){
        fprintf(stderr,"Error: Could not initialize Queues.\n");
        return 1;
    }
    directory_queue = *ptr1;
    threads_queue = *ptr2;

    //2. Put the search root directory (where to start the search, specified in argv[1]) in the queue
    if (insert(&directory_queue, root_directory)==PROBELM){
        free_queue(&directory_queue);
        fprintf(stderr,"Error: Error in isertion to queue.\n");
        return 1;
    }

    //Initialize all locks and condition Variables
    pthread_mutex_init(&all_initialized_lock, NULL);
    pthread_cond_init (&all_initialized_cond,NULL);

    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&thread_lock, NULL);
    pthread_cond_init (&empty_cond, NULL);
    pthread_cond_init (&first_out_cond, NULL);
    
    int* tid;
    //3. Create n searching threads (as per the number received in argv[3]).
    pthread_t threads[n];
    for (int i=0; i<n ;i++){
        tid = malloc(sizeof(int));
        if (tid==NULL){
            return finalize(1);
        }
        *tid = i;
        pthread_create(&threads[i],NULL,&searching_thread, (void*)tid);
    }

    //4.After all searching threads are created, the main thread signals them to start searching.
    
    for (int i=0; i<n ;i++){
        pthread_join(threads[i], NULL);
    }

    printf("Done searching, found %d files\n",count_found);

    if(error_threads >0 ){
        return finalize(1);
    }
    return finalize(0);
}
