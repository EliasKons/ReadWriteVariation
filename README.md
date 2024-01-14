The program is built with the command:
make

The program is built and run with the command:
make run

The program is built and run with valgrind with the command:
make valgrind

Object files and text files created by children are destroyed with the command:
make clean

Command line arguments can be changed in the makefile in the ARGS variable

The parent process first counts the number of lines in the text file. The segments are then separated by the lines/segments ratio (with the last segment to also take the lines resulting from lines%segments). There are 4 different memory attachments: 1 struct shared_memory for the semaphores, the segment number, the number of lines and the total requests that have been made, 1 mutex semaphore table with the same size as the segments, 1 semlock_string string that stores the segment as a string and 1 semlock_count integer array with the size of the segments to set the input and output of each segment in the shared memory and the critical section.

The struct shared_memory contains 3 semaphores: the rw_mutex, used to enter and exit the first children with different segments in critical section, and the requestConsumer and answerProducer for child-parent communication.

It then creates n children, calls the child function for each of them and initializes the segments using the strings text array.

A "request" is considered the inclusion of the segment number in the shared memory by the first child (or the theoretical inclusion of this number by the other children) and as an "answer" is considered the acquisition of the line by the process. From there, the placement for the calculations of the corresponding times is justified.

The parent works as follows: It waits for a request from a child (wait(requestConsumer)). In the special case where the maximum number of requests have been made, then it stops. Returns the requested segment and its number of lines in shared memory. Notifies the child that it has finished loading its segment (post(answerProducer)).

Children work as follows: They randomly choose one of the available segments. Children with shared segments are placed in a FIFO queue (wait(mutex[segment])). The first children from each segment also enter a FIFO queue (wait(rw_mutex)). The first child puts its segment in shared memory, makes a request to the parent(post(requestConsumer)) and waits for a response (wait(answerProducer)). It increases the number of requests by 1 and in the case that the requests exceed their limit, informs the parent to stop. Then all the children in the child's segment enter the critical section (post(mutex[segment])) and these in turn increase the requests and inform the parent in case it has to stop. Every child that is in the critical section takes a random line from the segment and prints it to its text file and finally waits for 20ms. These children come in in a FIFO queue (wait(mutex[segment])) and essentially exit it in turn immediately (post(mutex[segment])), except for the last child that leaves the next first child with a different segment to enter the critical section (post(rw_mutex)).

Finally, both the parent process and the children release the memory they committed and the memory segments are detached and destroyed.