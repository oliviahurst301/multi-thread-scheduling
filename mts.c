#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>

#define MAX_TRAINS 65
#define CONVERT_TO_US 100000

/*** REFERENCES ***

In addition to class slides and tutorials:

https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread.h.html used 
for extra information surrounding all pthread functions used in p2

https://pubs.opengroup.org/onlinepubs/007908799/headix.html used for 
information regarding the following libraries and associated functions
    unistd.h
        - usleep()
    sys/time.h
        - gettimeofday()

https://www.codecademy.com/resources/docs/c/enums used for examples
of defining enums


*/

typedef enum {
    NONE, // 0
    EAST, // 1
    WEST // 2
} Direction;

typedef enum {
    LOW, // 0 
    HIGH // 1
} Priority;

/* Info for one train including its loading status */
typedef struct {
    int number;
    Direction direction;
    Priority priority;
    int loading_time;
    int crossing_time;
    bool done_loading;
} Train;

/* Arguments passed to each train thread */
typedef struct {
    Train *train;
    Train *all_trains;
} ThreadArgs;

/* Circular queue of train pointers ordered by loading time and train ID */
typedef struct {
    Train *trains[MAX_TRAINS];
    int front;
    int rear;
    int count;
} TrainQueue;

/* Each station contains a high priority and low priority queue */
typedef struct {
    TrainQueue high_priority;
    TrainQueue low_priority;
} Station;

/* Shared state used to coordinate access to maint track */
Station east_station;
Station west_station;

/* Only one train can occupy track at a time */
bool track_occupied = false;

/* Track recent crossing for starvation rules */
Direction previous_direction = NONE;
int consecutive_trains = 0;

/* Protects shared queues, track state, start flag */
pthread_mutex_t scheduling_mutex;

/* Signals for changes in train readiness & track status */
pthread_cond_t start_convar;
pthread_cond_t scheduling_convar;
bool start_allowed = false;

/* Reference time for output timestamps */
struct timeval start_time;


/* Reset queue to empty state */
void initialize_queue(TrainQueue *queue) {
    queue -> front = 0;
    queue -> rear = -1;
    queue -> count = 0;
}

/***  Customized queue operation implementations 
based on basic queue operation implementations provided by:
https://www.geeksforgeeks.org/c/c-program-to-implement-circular-queue/ ***/

/* Insert by loading time
   Break ties w/ lower train ID */
void enqueue(TrainQueue *queue, Train *train) {
    if (queue -> count == 0) {
        queue -> rear = (queue -> rear + 1) % MAX_TRAINS;
        queue -> trains[queue -> rear] = train;
        queue -> count++;
        return;
    }
    
    /* Fine where train belongs in queue order */
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

    /* Shift later entries by one to make room */
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

/* Remove & return train at front of queue */
Train *dequeue(TrainQueue *queue) {
    if (queue -> count == 0) {
        return NULL;
    }

    Train *removed_train = queue -> trains[queue -> front];
    queue -> front = (queue -> front + 1) % MAX_TRAINS;
    queue -> count--;

    return removed_train;
}

/* Return train at front w/o removing it */
Train *peek(TrainQueue *queue) {
    if (queue -> count == 0) {
        return NULL;
    }

    return queue -> trains[queue -> front];
}

/* Place loaded train in queue for departure station and priority */
void queue_train(Train *train) {

    /* Eastbound trains depart from west station */
    if (train -> direction == EAST) {

        /* Determine priority level */
        if (train -> priority == LOW) {
            enqueue(&west_station.low_priority, train);
        }
        else {
            enqueue(&west_station.high_priority, train);
        }
    }
    /* Westbound trains depart from east station */
    else {
        if (train -> priority == LOW) {
            enqueue(&east_station.low_priority, train);
        }
        else {
            enqueue(&east_station.high_priority, train);
        }
    }
}

/* Choose next eligible train w/o removing it from queue */
Train *select_next_train() {

    /* High priority takes precedence */
    bool high_priority_waiting = 
        east_station.high_priority.count > 0 || 
        west_station.high_priority.count > 0;

    /* Pointers to queues that will be considered for scheduling */
    TrainQueue *east_station_queue;
    TrainQueue *west_station_queue;

    /* Select high priority queues if any trains waiting */
    if (high_priority_waiting) {
        east_station_queue = &east_station.high_priority;
        west_station_queue = &west_station.high_priority;
    }
    /* Otherwise select low priority queues */
    else {
        east_station_queue = &east_station.low_priority;
        west_station_queue = &west_station.low_priority;
    }

    /* Ensure starvation rule followed */
    if (consecutive_trains >= 2) {
        if (previous_direction == EAST) {
            if (east_station.high_priority.count > 0) {
                return peek(&east_station.high_priority);
            }
            else if (east_station.low_priority.count > 0) {
                return peek(&east_station.low_priority);
            }
        }
        else if (previous_direction == WEST) {
            if (west_station.high_priority.count > 0) {
                return peek(&west_station.high_priority);
            }
            else if (west_station.low_priority.count > 0) {
                return peek(&west_station.low_priority);
            }
        }
    }

    /* Look at first train in each selected queue w/o removing it 
       Eastbound trains queued at west station
       Westbound trains queued at east station */
    Train *eastbound_train = peek(west_station_queue);
    Train *westbound_train = peek(east_station_queue);

    /* If only one direction has a train waiting, select it */
    if (eastbound_train == NULL && westbound_train != NULL) {
        return westbound_train;
    }
    
    if (westbound_train == NULL && eastbound_train != NULL) {
        return eastbound_train;
    }

    /* No trains waiting */
    if (westbound_train == NULL && eastbound_train == NULL) {
        return NULL;
    }

    /* If both directions have waiting trains of same priority:
        - choose direction opposite to previous crossing
        - if no train cross yet, choose eastbound */
    if (previous_direction == EAST) {
        return westbound_train;
    }
    else {
        return eastbound_train;
    }
}

/* Read up to 65 entires
   Assign ID's in input-file order */
void read_input_file(FILE *input_file, Train trains[], int *train_count) {

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

/* Print elapsed simulation time as HH:MM:SS.t */
void print_timestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    long time_elapsed_us = 
        (current_time.tv_sec - start_time.tv_sec) * 1000000L + 
        (current_time.tv_usec - start_time.tv_usec);

    /* Convert elapsed microseconds into tenths */
    long total_tenths = time_elapsed_us / 100000;

    long hours = total_tenths / 36000;
    long minutes = (total_tenths / 600) % 60;
    long seconds = (total_tenths / 10) % 60;
    long tenths = total_tenths % 10;

    printf("%02ld:%02ld:%02ld.%01ld ", hours, minutes, seconds, tenths);
}

/* Simulate loading, enqueue train, accounce when it's ready */
void prepare_train(Train *train) {
    
    /* Train sleeps for specified loading time in microseconds */
    usleep(train -> loading_time * CONVERT_TO_US);

    /* Acquire mutex before accessing queue insertion and loading state */
    pthread_mutex_lock(&scheduling_mutex);

    queue_train(train);

    train -> done_loading = true;

    /* Wake threads that may be eligible for crossing */
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

    /* Release mutex */
    pthread_mutex_unlock(&scheduling_mutex);
}

/* Prevent race conditions when two trains have same direction, priority, loading time */
bool wait_for_earlier_train(Train *train, Train trains[]) {

    /* Train w/ lower ID number but other matching attributes must load first */
    for (int i = 0; i < train -> number; i++) {
        if (trains[i].direction == train -> direction && 
            trains[i].priority == train -> priority && 
            trains[i].loading_time == train -> loading_time) {
                if (trains[i].done_loading == false) {
                    return true;
                }
        }
    }
    /* Otherwise current train may cross */
    return false;
}

/* Wait until this train selected and track is free, then enter it */
void check_crossing(Train *train, Train trains[]) {

    /* Acquire mutex before accessing track state and train queues */
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

    /* Remove selected train from its station's queue */
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

    /* Claim track before allowing other threads to schedule */
    track_occupied = true;

    print_timestamp();
    printf(
        "Train %2d is ON the main track going %4s\n", 
        train -> number, train -> direction == EAST ? "East" : "West");

    pthread_mutex_unlock(&scheduling_mutex);
}

/* Wait for common start, load, cross, and finish */
void *process_train(void *arg) {

    /* Recover thread's train & array used for tie-breaking */
    ThreadArgs *args = (ThreadArgs *)arg;
    Train *train = args -> train;
    Train *all_trains = args -> all_trains;

    /* Wait for main to create all threads and set start time */
    pthread_mutex_lock(&scheduling_mutex);

    /* While waiting for all threads to be created:
        - temporarily release mutex
        - put thread to sleep while waiting for signal
        - reacquire mutex once woken                 */
    while (start_allowed == false) {
        pthread_cond_wait(&start_convar, &scheduling_mutex);
    }

    /* Begin loading & queuing, check crossing status */
    pthread_mutex_unlock(&scheduling_mutex);
    prepare_train(train);
    check_crossing(train, all_trains);

    /* Put train to sleep for crossing time in microseconds */
    usleep(train -> crossing_time * CONVERT_TO_US);

    /* Protect completion update, release of track */
    pthread_mutex_lock(&scheduling_mutex);

    /* Count consecutive crossings in same direction */
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

    return NULL;
}

int main(int argc, char *argv[]) {
    
    Train trains[MAX_TRAINS]; /* Train structs */
    ThreadArgs thread_args[MAX_TRAINS]; /* Thread argument structs */
    pthread_t train_threads[MAX_TRAINS]; /* Thread identifiers */

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

    /* File contains no trains */
    if (train_count == 0) {
        return 0;
    }

    /* Initialize station queues */
    initialize_queue(&east_station.high_priority);
    initialize_queue(&east_station.low_priority);
    initialize_queue(&west_station.high_priority);
    initialize_queue(&west_station.low_priority);

    /* Initialize scheduling mutex and convars with error checking */
    if (pthread_mutex_init(&scheduling_mutex, NULL) != 0) {
        return 1;
    }
    if (pthread_cond_init(&start_convar, NULL) != 0) {
        pthread_mutex_destroy(&scheduling_mutex);
        return 1;
    }
    if (pthread_cond_init(&scheduling_convar, NULL) != 0) {
        pthread_cond_destroy(&start_convar);
        pthread_mutex_destroy(&scheduling_mutex);
        return 1;
    }

    /* Create a thread for each train, add it to train_threads[] */
    for (int i = 0; i < train_count; i++) {
        thread_args[i].train = &trains[i];
        thread_args[i].all_trains = trains;

        if (pthread_create(&train_threads[i], NULL, process_train, &thread_args[i]) != 0) {
            exit(1);
        }
    }

    /* Acquire mutex before changing shared variable */
    pthread_mutex_lock(&scheduling_mutex);

    /* Record simulation start time */
    gettimeofday(&start_time, NULL);

    /* Allow all train threads to begin loading */
    start_allowed = true;
    pthread_cond_broadcast(&start_convar);

    pthread_mutex_unlock(&scheduling_mutex);

    /* Make main() wait for ALL train threads to finish before continuing */
    for (int i = 0; i < train_count; i++) {
        if (pthread_join(train_threads[i], NULL) != 0) {
            exit(1);
        }
    }

    /* Destory mutex and convars */
    int cleanup_failed = 0;
    
    pthread_mutex_destroy(&scheduling_mutex);
    pthread_cond_destroy(&start_convar);
    pthread_cond_destroy(&scheduling_convar);
    
    return 0;
}