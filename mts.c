#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_TRAINS 65
#define CONVERT_TO_US 100000

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

bool track_occupied = false;
Direction previous_direction = NONE;
int consecutive_trains = 0;
pthread_mutex_t scheduling_mutex;
pthread_cond_t start_convar;
pthread_cond_t scheduling_convar;
bool start_allowed = false;

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

Train *select_next_train() {

    /* Check if there are any high priority trains waiting */
    bool high_priority_waiting = east_station.high_priority.count > 0 || west_station.high_priority.count > 0;

    /* Pointers to queues that will be considered for scheduling */
    TrainQueue *east_station_queue;
    TrainQueue *west_station_queue;

    /* 
    Select high priority queues if any trains waiting 
    Otherwise select low priority queues */
    if (high_priority_waiting) {
        east_station_queue = &east_station.high_priority;
        west_station_queue = &west_station.high_priority;
    }
    else {
        east_station_queue = &east_station.low_priority;
        west_station_queue = &west_station.low_priority;
    }

    /* Look at first train in each selected queue w/o removing it 
       Trains depart from station opposite their travelling direction */
    Train *eastbound_train = peek(west_station_queue);
    Train *westbound_train = peek(east_station_queue);

    /* If only one direction has a train waiting select that train */
    if (eastbound_train == NULL && westbound_train != NULL) {
        return westbound_train;
    }
    
    if (westbound_train == NULL && eastbound_train != NULL) {
        return eastbound_train;
    }

    /* If neither direction has a train waiting, don't select anything */
    if (westbound_train == NULL && eastbound_train == NULL) {
        return NULL;
    }

    /* If both directions have waiting trains of same priority:
        - choose direction opposite to previous crossing
        - if no train cross yet, choose eastbound           */
    if (previous_direction == EAST) {
        return westbound_train;
    }
    else {
        return eastbound_train;
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

void prepare_train(Train *train) {
    
    /* Train sleeps for specified loading time in microseconds */
    usleep(train -> loading_time * CONVERT_TO_US);

    /* Acquire mutex before entering correct FIFO queue, wait if already acquired */
    pthread_mutex_lock(&scheduling_mutex);

    queue_train(train);

    printf("Train %d finished loading\n", train -> number);

    /* Release mutex so another thread may acquire it */
    pthread_mutex_unlock(&scheduling_mutex);

    /*
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

void check_crossing(Train *train) {

    /* Acquire mutex before accessing shared variable */
    pthread_mutex_lock(&scheduling_mutex);

    /* While track is occupied or another train has priority:
       - temporarily release mutex
       - put thread to sleep while waiting for signal
       - reacquire mutex once woken                     */
    while (track_occupied || select_next_train() != train) {
        pthread_cond_wait(&scheduling_convar, &scheduling_mutex);
    }

    /* Remove selected train from its station's queue 
       Eastbound trains depart from west station
       Westbound trains depart from east station */
    if (train -> direction == EAST && train -> priority == HIGH) {
        dequeue(&west_station.high_priority);
    }                                                
    else if (train -> direction == EAST && train -> priority == LOW) {
        dequeue(&west_station.low_priority);
    }
    else if (train -> direction == WEST && train -> priority == HIGH) {
        dequeue(&east_station.high_priority);
    }
    else if (train -> direction == WEST && train -> priority == LOW) {
        dequeue(&east_station.low_priority);
    }

    /* Train done waiting, claims track */
    track_occupied = true;

    pthread_mutex_unlock(&scheduling_mutex);
    
    /*
    train checks if it can cross per scheduling rules
    
    while train cannot cross:
        - wait on scheduling convar
        - check rules again
    
    remove train from queue
    mark track occupied
    return with train ready to cross
    */
}

void *process_train(void *arg) {

    /* Convert generic arg to a Train pointer */
    Train *train = (Train *)arg;

    /* Acquire mutex before accessing shared variable */
    pthread_mutex_lock(&scheduling_mutex);

    /* While waiting for all threads to be created:
        - temporarily release mutex
        - put thread to sleep while waiting for signal
        - reacquire mutex once woken                 */
    while (start_allowed == false) {
        pthread_cond_wait(&start_convar, &scheduling_mutex);
    }

    /* Release mutex, begin loading & queuing */
    pthread_mutex_unlock(&scheduling_mutex);
    prepare_train(train);

    /* 
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
    return NULL;
}

int main(int argc, char *argv[]) {
    
    /* Array of Train structs, threads (one per train) */
    Train trains[MAX_TRAINS];
    pthread_t train_threads[MAX_TRAINS];

    /* Counter to keep track of amt of trains in trains[] */
    int train_count = 0;

    /* Check to make sure user entered argument in the form "./mts filename" */
    if (argc < 2) {
        return 1;
    }

    FILE *file = fopen(argv[1], "r");

    /* Ensure file contains input */
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

    /* Initialize station queues */
    initialize_queue(&east_station.high_priority);
    initialize_queue(&east_station.low_priority);
    initialize_queue(&west_station.high_priority);
    initialize_queue(&west_station.low_priority);

    /* Initialize scheduling mutex, convars */
    pthread_mutex_init(&scheduling_mutex, NULL);
    pthread_cond_init(&start_convar, NULL);
    pthread_cond_init(&scheduling_convar, NULL);

    /* Create a thread for each train, add it to train_threads[] */
    for (int i = 0; i < train_count; i++) {
        pthread_create(&train_threads[i], NULL, process_train, &trains[i]);
    }

    /* 
    Acquire mutex before changing shared variable
    Wake all threads waiting on start_allowed == true
    Release mutex 
    */
    pthread_mutex_lock(&scheduling_mutex);

    start_allowed = true;

    pthread_cond_broadcast(&start_convar);

    pthread_mutex_unlock(&scheduling_mutex);

    /* Make main() wait for ALL train threads to finish before continuing */
    for (int i = 0; i < train_count; i++) {
        pthread_join(train_threads[i], NULL);
    }

    /* Destory mutex and convars */
    pthread_mutex_destroy(&scheduling_mutex);
    pthread_cond_destroy(&start_convar);
    pthread_cond_destroy(&scheduling_convar);
    
    /*
    destroy convars
    */

    return 0;
}