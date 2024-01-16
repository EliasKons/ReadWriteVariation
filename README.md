# ReadWriteVariation

This program reads a text file using multiple child processes, each handling a specific segment of the file.

## Building the Program

The program can be built using the following commands:

- Build: `make`
- Build and Run: `make run`
- Build and Run with Valgrind: `make valgrind`
- Clean: `make clean`

Command line arguments can be modified in the `makefile` under the `ARGS` variable.

## Program Overview

The parent process counts the number of lines in the text file and separates segments based on the lines/segments ratio. The program utilizes multiple memory attachments for synchronization:

1. **Shared Memory Struct**: Contains semaphores for managing access to shared resources such as the semaphore table, string segment, and count array.
2. **Mutex Semaphore Table**: Sized according to the number of segments to control access to each segment.
3. **Semlock String**: Ensures exclusive access to the string segment.
4. **Semlock Count**: Integer array used to set input and output of each segment in shared memory.

The shared memory struct includes three semaphores:

- `rw_mutex`: Allows the first child with different segments to enter and exit the critical section.
- `requestConsumer`: Manages child-parent communication for requests.
- `answerProducer`: Manages child-parent communication for answers.

## Parent Process

1. Waits for a request from a child (`wait(requestConsumer)`).
2. If the maximum number of requests is reached, the parent stops; otherwise, it returns the requested segment and its number of lines in shared memory.
3. Notifies the child that it has finished loading its segment (`post(answerProducer)`).

## Child Processes

1. Randomly choose an available segment.
2. Children with shared segments enter a FIFO queue (`wait(mutex[segment])`).
3. The first child from each segment enters a FIFO queue (`wait(rw_mutex)`), puts its segment in shared memory, makes a request to the parent (`post(requestConsumer)`), and waits for a response (`wait(answerProducer)`).
4. Increments the number of requests; if requests exceed the limit, informs the parent to stop.
5. All children in the segment enter the critical section (`post(mutex[segment])`), increase requests, and inform the parent if it needs to stop.
6. Each child in the critical section takes a random line from the segment, prints it to its text file, and waits for 20ms.
7. Children exit the critical section in a FIFO queue (`wait(mutex[segment])`), except the last child that allows the next first child with a different segment to enter the critical section (`post(rw_mutex)`).

## Cleaning Up

Both the parent process and the children release the committed memory, detach, and destroy the memory segments.
