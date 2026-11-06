#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>

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
    bool done_loading;
} Train;

typedef struct {
    Train *train;
    Train *all_trains;
} ThreadArgs;

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
struct timeval start_time;

/* Mutex */
/* Condition variables */

void initialize_queue(TrainQueue *queue) {
    queue -> front = 0;
    queue -> rear = -1;
    queue -> count = 0;
}

void enqueue(TrainQueue *queue, Train *train) {
    if (queue -> count == 0) {
        queue -> rear = (queue -> rear + 1) % MAX_TRAINS;
        queue -> trains[queue -> rear] = train;
        queue -> count++;
        return;
    }
        
    int insert_position = queue -> count;

    for (int i = 0; i < queue -> count; i++) {
        int index = (queue -> front + i) % MAX_TRAINS;
        Train *current_train = queue -> trains[index];

        if (train -> loading_time < current_train -> loading_time || 
            (train -> loading_time == current_train -> loading_time && 
            train -> number < current_train -> number)) {
            insert_position = i;
            break;
        }
    }

    for (int i = queue -> count; i > insert_position; i--) {
        int destination_index = (queue -> front + i) % MAX_TRAINS;
        int source_index = (queue -> front + i - 1) % MAX_TRAINS;
        queue -> trains[destination_index] = queue -> trains[source_index];
    }

    int insertion_index = (queue -> front + insert_position) % MAX_TRAINS;
    queue -> trains[insertion_index] = train;
    queue -> rear = (queue -> rear + 1) % MAX_TRAINS;
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

    /* Ensure starvation rule followed */
    if (consecutive_trains >= 2) {
        if (previous_direction == EAST) {
            if (east_station.high_priority.count >= 1) {
                return peek(&east_station.high_priority);
            }
            else if (east_station.low_priority.count >= 1) {
                return peek(&east_station.low_priority);
            }
        }
        else if (previous_direction == WEST) {
            if (west_station.high_priority.count >= 1) {
                return peek(&west_station.high_priority);
            }
            else if (west_station.low_priority.count >= 1) {
                return peek(&west_station.low_priority);
            }
        }
    }

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
        trains[*train_count].done_loading = false;

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

void print_timestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    long time_elapsed_us = 
        (current_time.tv_sec - start_time.tv_sec) * 1000000L + 
        (current_time.tv_usec - start_time.tv_usec);

    long total_tenths = time_elapsed_us / 100000;

    long hours = total_tenths / 36000;
    long minutes = (total_tenths / 600) % 60;
    long seconds = (total_tenths / 10) % 60;
    long tenths = total_tenths % 10;

    printf("%02ld:%02ld:%02ld.%01ld ", hours, minutes, seconds, tenths);
}

void prepare_train(Train *train) {
    
    /* Train sleeps for specified loading time in microseconds */
    usleep(train -> loading_time * CONVERT_TO_US);

    /* Acquire mutex before entering correct FIFO queue, wait if already acquired */
    pthread_mutex_lock(&scheduling_mutex);

    queue_train(train);

    train -> done_loading = true;

    /* Notify waiting trains after a train loads & enters queue */
    pthread_cond_broadcast(&scheduling_convar);

    /* Create string version of direction enum for output */
    const char *direction_string;

    if (train -> direction == EAST) {
        direction_string = "East";
    }
    else {
        direction_string = "West";
    }

    print_timestamp();
    printf("Train %2d is ready to go %4s\n", train -> number, direction_string);

    /* Release mutex so another thread may acquire it */
    pthread_mutex_unlock(&scheduling_mutex);
}

bool wait_for_earlier_train(Train *train, Train trains[]) {

    /* If a train that appears earlier in the input than current train and has same
       direction, priority and loading time, current train must wait for that earlier 
       train to load before crossing if not done already */
    for (int i = 0; i < train -> number; i++) {
        if (trains[i].direction == train -> direction && 
            trains[i].priority == train -> priority && 
            trains[i].loading_time == train -> loading_time) {
                if (trains[i].done_loading == false) {
                    return true;
                }
        }
    }
    /* Otherwise train may cross */
    return false;
}

void check_crossing(Train *train, Train trains[]) {

    /* Acquire mutex before accessing shared variable */
    pthread_mutex_lock(&scheduling_mutex);

    /* While track is occupied or another train has priority:
       - temporarily release mutex
       - put thread to sleep while waiting for signal
       - reacquire mutex once woken                     */
    while (track_occupied || 
        select_next_train() != train || 
        wait_for_earlier_train(train, trains)) {
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

    print_timestamp();
    printf(
        "Train %2d is ON the main track going %4s\n", 
        train -> number, train -> direction == EAST ? "East" : "West");

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

    /* Convert generic arg to a ThreadArgs pointer
       Access pointer to train and all_trains through args */
    ThreadArgs *args = (ThreadArgs *)arg;
    Train *train = args -> train;
    Train *all_trains = args -> all_trains;

    /* Acquire mutex before accessing shared variable */
    pthread_mutex_lock(&scheduling_mutex);

    /* While waiting for all threads to be created:
        - temporarily release mutex
        - put thread to sleep while waiting for signal
        - reacquire mutex once woken                 */
    while (start_allowed == false) {
        pthread_cond_wait(&start_convar, &scheduling_mutex);
    }

    /* Release mutex, begin loading & queuing, check crossing status */
    pthread_mutex_unlock(&scheduling_mutex);
    prepare_train(train);
    check_crossing(train, all_trains);

    /* Put train to sleep for crossing time in microseconds */
    usleep(train -> crossing_time * CONVERT_TO_US);

    pthread_mutex_lock(&scheduling_mutex);

    /* If direction of train that just crossed == direction of previous train, inc consecutive_train
       Otherwise consecutive_train = 1 (reset count) */
    if (train -> direction == previous_direction) {
        consecutive_trains++; 
    }
    else {
        consecutive_trains = 1;
    }

    /* Update previous direction */
    previous_direction = train -> direction;

    print_timestamp();
    printf(
        "Train %2d is OFF the main track after going %4s\n", 
        train -> number, train -> direction == EAST ? "East" : "West");

    track_occupied = false;

    /* Signal threads waiting on scheduling, release mutex */
    pthread_cond_broadcast(&scheduling_convar);
    pthread_mutex_unlock(&scheduling_mutex);

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
    
    Train trains[MAX_TRAINS]; /* Train structs */
    ThreadArgs thread_args[MAX_TRAINS]; /* Thread identifiers */
    pthread_t train_threads[MAX_TRAINS]; /* Argument structs */

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
        thread_args[i].train = &trains[i];
        thread_args[i].all_trains = trains;
        pthread_create(&train_threads[i], NULL, process_train, &thread_args[i]);
    }

    /* 
    Acquire mutex before changing shared variable
    Wake all threads waiting on start_allowed == true
    Release mutex 
    */
    pthread_mutex_lock(&scheduling_mutex);

    /* Start simulation before waking all threads */
    gettimeofday(&start_time, NULL);

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