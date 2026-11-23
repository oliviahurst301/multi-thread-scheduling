# Multithreaded Train Scheduler

A multithreaded train scheduling simulation developed in C using POSIX threads (pthreads). The program simulates trains arriving at two stations and coordinates their movement across a shared, single-track railway. This project focuses on concurrency, thread synchronization, priority-based scheduling, and starvation prevention.

# Features
- Multithreading: Each train is represented by an independent thread, allowing trains to load concurrently.
- Priority-Based Scheduling: High-priority trains are generally scheduled before low-priority trains.
- FIFO Queue Management: Trains are organized by loading time, with train ID used to break ties.
- Direction Alternation: When eligible trains are waiting in both directions, the scheduler generally favours the direction opposite the   previous crossing.
- Starvation Prevention: After two consecutive trains cross in the same direction, a waiting train travelling in the opposite direction    is given an opportunity to cross.
- Thread Synchronization: Mutexes and condition variables coordinate access to shared scheduling data and ensure only one train occupies   the track at a time.

# Technologies
- Language: C
- Concurrency: POSIX Threads (pthreads)
- Synchronization: Mutexes and condition variables
- Development Environment: Linux
- Build Tool: Make

# How It Works
The simulation reads train information from an input file. Each train has a direction, priority level, loading time, and crossing time.
1. Initialization: The program creates a thread for each train and synchronizes their starting point.
2. Loading: Each train independently simulates its loading time before entering the appropriate station queue.
3. Scheduling: The scheduler selects the next eligible train based on priority, loading order, direction, and starvation prevention         rules.
4. Crossing: The selected train acquires exclusive access to the main track and simulates its crossing time.
5. Completion: Once the train finishes crossing, it releases the track and notifies waiting threads so another train can proceed.
The program outputs timestamped messages indicating when each train finishes loading, enters the main track, and completes its crossing.

# Running the Program
Requirements:
- Linux environment
- GCC compiler
- POSIX threads support
- Make

# Compilation
From the project directory, run: 

    make

# Execution
Run the program with an input file: 

    ./mts input.txt

Each line of the input file follows this format: 

    direction loading_time crossing_time
- E / W: High-priority eastbound or westbound train
- e / w: Low-priority eastbound or westbound train
- loading_time: Time required to load, in tenths of a second
- crossing_time: Time required to cross, in tenths of a second

Example input:

    E 2 5
    w 1 3
    e 4 2
    W 3 4

# Key Learning Outcomes

Through this project, I gained practical experience with:
- Creating and managing multiple threads in C.
- Using mutexes to protect shared data and prevent race conditions.
- Coordinating threads using condition variables.
- Implementing scheduling rules while maintaining fairness between competing threads.
- Debugging concurrent programs, where thread execution order can vary between runs.
- Testing scheduling behaviour under different combinations of train priorities, directions, and loading times.

# Project Background

Developed as part of CSC 360: Operating Systems at the University of Victoria. The implementation demonstrates core operating systems concepts through a practical concurrency and scheduling problem.
