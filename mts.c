#include <pthread.h>
#include <stdlib.h>

#define MAX_TRAINS 65

/* Train struct */
typedef struct {
    int number;
    char direction;
    int priority;
    int loading_time;
    int crossing_time;
} Train;

/* train queue struct */
typedef struct {
    Train *trains[MAX_TRAINS];
    int front;
    int rear;
    int count;
} TrainQueue;

/* station struct */
typedef struct {
    TrainQueue high_priority;
    TrainQueue low_priority;
} Station;

/* Shared scheduling state */
Station east_station;
Station west_station;

int track_occupied = 0;
char previous_direction = '\0';
int consectutive_trains = 0;

/* Mutex */
/* Condition variables */

void initialize_queue(TrainQueue *queue) {
    queue -> front = 0;
    queue -> rear = -1;
    queue -> count = 0;
}

void enqueue(TrainQueue *queue, Train *train) {
    queue -> rear = (queue -> rear + 1) % MAX_TRAINS;
    queue -> trains[queue -> rear] = train;
    queue -> count++;
}

Train *dequeue(TrainQueue *queue) {
    if (queue -> count == 0) {
        return NULL;
    }

    Train *removed_train = queue -> trains[queue -> front];
    queue -> front = (queue -> front + 1) % MAX_TRAINS;
    queue -> count--;

    return removed_train;
}

Train *peek(TrainQueue *queue) {
    if (queue -> count == 0) {
        return NULL;
    }

    return queue -> trains[queue -> front];
}

void read_input_file() {
    /*
    read input
    parse args
    populate train struct array
    */
}

void load_and_queue() {
    /*
    train sleeps for specified loading time
    
    once awake:
        - acquire mutex
        
        *** critical section ***
        - enter correct FIFO queue based on direction and priority
        - print simulation time, "Train [train #] is ready to go [Direction]"
        *** end of critical section ***

        - release mutex
    
    now check crossing
    */
}

void check_crossing() {
    /*
    acquire mutex if necessary 

    train checks if it can cross per scheduling rules
    
    while train cannot cross:
        - wait on scheduling convar
        - check rules again
    
    remove train from queue
    mark track occupied
    return with train ready to cross
    */
}

void train_thread() {
    /* 
    wait for simulation to start
    load_and_queue()
    check_crossing()
    print train ON track
    release mutex
    sleep for crossing time
    reacquire mutex
    mark track unoccupied
    print train OFF track
    update:
        - previous direction
        -consectutive train information
    wait waiting trains
    release mutex
    terminate thread
    */
}

void main() {
    /* 
    open file
    call function to read file, parse args

    initialize:
        - station queue
        - shared scheduling state
        - mutex
        - convars

    create one train_thread for each train 
    
    start simulation time
    wake train threads

    join all train threads

    destroy mutex and convars

    return
    */
}