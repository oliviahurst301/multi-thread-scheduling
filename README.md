# p2 Design

1. In addition to the main thread, one thread will be created for each train in the input file. Each train thread will:
    - wait for the simulation to start
    - simulate its loading time
    - add itself to the correct station queue once loaded
    - wait until its allowed to cross according to scheduling rules
    - simulate its crossing time once allowed
    - terminate after crossing the track

2. 
    - There will be no overall controller thread. 
    - Each train thread will handle its own loading, waiting, and crossing. 
    - Once loaded, train threads will coordinate using the queues, mutex, and condition variables. 
    - Scheduling rules will determine when each waiting train is eligible to cross.

3. One mutex will be used for the shared scheduling state. It will protect:
    - the four station queues
    - the state of the main track (occupied or unoccupied)
    - information needed for scheduling such as the direction of the previously dispatched trains and the number of consecutive trains
      travelling in that direction

   A train must acquire this mutex before reading or modifying any of this shared information. The mutex will not remain locked while a train
   is crossing the main track, instead the track will be marked as occupied before the mutex is released while the crossing occurs.

4. The main thread won't be involved in the scheduling of trains. It will:
    - read the input file
    - initialize the queues, mutex, and condition variables
    - create the train threads (one per train)
    - signal the train threads to begin once they've all been created (i.e. so simulation start time is the same for all)
    - wait for all train threads to finish using pthread_join()
    - perform cleanup and terminate once all other trains have finished

5. The two stations will be represented using structs. Each station will have two FIFO queues for the two types of priority. This results in   
   four queues total:
    - Eastbound high priority (E)
    - Eastbound low priority (e)
    - Westbound high priority (W)
    - Westbound low priorty (w)

   The West station will contain the eastbound queues and the East station will contain the westbound queues, since a train's direction
   describes where its travelling, not where it starts. Once a train is done loading, it's added to the appropriate queue. Trains within a 
   queue will be ordered by when they finish loading. If two trains finish loading at the same time, their order in the input file will break
   the tie.

6. The shared scheduling mutex will prevent concurrent modification. A train must acquire the mutex before accessing or modifying:
   - any of the four queues
   - track availability
   - other shared scheduling information

   Once it's finished accessing/modifying the shared data, it will release the mutex so another train thread can acquire it.

7. Two condition variables will be used: start condition variable and scheduling condition variable

   Start condition variable
    - condition: whether the simulation has started
    - associated mutex: the shared scheduling mutex
    - each train initially waits on this convar
    - once the main thread has created all train threads, it sets the simulation as started and wakes them
    - after a train wakes and reacquires the mutex, it can start its loading process

   Scheduling condition variable
    - condition: whether the scheduling state has changed such that a waiting may be allowed to cross
    - associated mutex: the shared scheduling mutex as the condition depends on queues, track availability, and previous crossing information
    - a loaded train waits on this convar if it's not currently eligible to cross
    - once awakened and after reacquiring the mutex, the train can check the scheduling rules again
        - if eligible -> mark track as occupied and continue
        - if ineligible -> wait on convar again

8. Overall sample algorithm:

     1     main reads input file and creates thread for each train
     2     trains wait on start convar
     3     once all threads created, main starts simulation and wakes all train threads
     4     trains sleep for specified loading times
     5     once loaded, train acquires mutex and enters corresponding FIFO queue
     6     train checks if it can cross per scheduling rules
     7     if train can't cross, wait on scheduling convar and check again when awakened
     8     if train can cross, remove self from queue and mark track as occupied
     9     train releases mutex, sleeps during crossing time
    10     after crossing, train reacquires mutex and marks track as unoccupied
    11     train updates previous direction and consecutive-train information
    12     train wakes waiting trains so they can check scheduling rules
    13     train releases mutex and terminates
    14     main joins all threads, terminates after every train has finished