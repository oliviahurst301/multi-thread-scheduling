#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_TRAINS 65

typedef enum {
    NONE, // 0
    EAST, // 1
    WEST // 2
} Direction;

typedef enum {
    LOW, // 0 
    HIGH // 1
} Priority;

/* Train struct */
typedef struct {
    int number;
    Direction direction;
    Priority priority;
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
Direction previous_direction = NONE;
int consecutive_trains = 0;

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

void queue_train(Train *train) {

    /* Check if train is travelling East */
    if (train -> direction == EAST) {

        /* Determine priority level, add to appropriate West station queue */
        if (train -> priority == LOW) {
            enqueue(&west_station.low_priority, train);
        }
        else {
            enqueue(&west_station.high_priority, train);
        }
    }
    /* Train is travelling West, add to appropriate East station queue*/
    else {
        if (train -> priority == LOW) {
            enqueue(&east_station.low_priority, train);
        }
        else {
            enqueue(&east_station.high_priority, train);
        }
    }
}

void read_input_file(FILE *input_file, Train trains[], int *train_count) {
    /*
    read input
    parse args
    populate train struct array
    */

    char direction_and_priority;
    int loading_time;
    int crossing_time;

    while (*train_count < MAX_TRAINS && fscanf(input_file, " %c %d %d", &direction_and_priority, &loading_time, &crossing_time) == 3) {
        trains[*train_count].number = *train_count;
        trains[*train_count].loading_time = loading_time;
        trains[*train_count].crossing_time = crossing_time;

        /* Assign appropriate priority */
        if (isupper(direction_and_priority)) {
            trains[*train_count].priority = HIGH;
        } 
        else {
            trains[*train_count].priority = LOW;
        }

        /* Assign appropriate direction */
        if (toupper(direction_and_priority) == 'E') {
            trains[*train_count].direction = EAST;
        }
        else {
            trains[*train_count].direction = WEST;
        }
        
        (*train_count)++;
    }
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

void *train_thread(void *arg) {
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

int main(int argc, char *argv[]) {
    
    /* Array of Train structs */
    Train trains[MAX_TRAINS];

    /* Counter to keep track of amt of trains in trains[] */
    int train_count = 0;
   
    /* 
    open file
    call function to read file, parse args
    */

    /* Check to make sure user entered argument in the form "./mts filename" */
    if (argc < 2) {
        return 1;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL) {
        return 1;
    } 
    
    /* Read input file, load array with train structs */
    read_input_file(file, trains, &train_count);

    int file_closed = fclose(file);

    /* Ensure file closed correctly */
    if (file_closed == -1) {
        return -1;
    } 

    /*
    initialize:
        - station queue
        - shared scheduling state
        - mutex
        - convars
    */

    initialize_queue(&east_station.high_priority);
    initialize_queue(&east_station.low_priority);
    initialize_queue(&west_station.high_priority);
    initialize_queue(&west_station.low_priority);

    /*
    create one train_thread for each train 
    
    start simulation time
    wake train threads

    join all train threads

    destroy mutex and convars

    return
    */
    return 0;
}