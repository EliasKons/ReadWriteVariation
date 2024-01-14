#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include "shared_memory.h"

void child(int requests, int segments, int n, int max_length, sharedMemory semlock_info, char* semlock_string, int* semlock_count, sem_t *mutex) {
    int i, request_count, segment, previous_segment, line, length = 0;
    static int total_requests;
    char mypid[10], *textLine, *curLine, *nextLine, *temp;
    FILE *fptr;
    struct timeval current_time;
    
    sprintf(mypid, "%d", getpid()); 
    strcat(mypid, ".txt");
    fptr = fopen(mypid, "w"); // file creation with id as name

    srand((unsigned)time(NULL) - getpid());
    segment = rand()%segments; // starting segment

    textLine = malloc((max_length + 1)* sizeof(char)); // line of interest

    total_requests = n*requests; // number of total requests from every child

    /* start of communcation between child and parent */
    for(request_count = 0; request_count < requests; request_count++) {
        sem_wait(&mutex[segment]); // wait for children that chose the same segment
        semlock_count[segment]++; // counter used to manage the order of the children of the same segement

        gettimeofday(&current_time, NULL); // time of request
        fprintf(fptr, "Time of request:%ld\n", current_time.tv_usec);

        if(semlock_count[segment] == 1) { // first child of the segment
            sem_wait(&semlock_info->rw_mutex); // other segments have to wait
            semlock_info->segment = segment;  // pass segment to the shared memory
            
            sem_post(&semlock_info->requestConsumer); // request 
            sem_wait(&semlock_info->answerProducer); // wait for answer
        }
        
        semlock_info->requests++; // another "request" done
        if(semlock_info->requests == total_requests)  // case where parent has to stop
            sem_post(&semlock_info->requestConsumer);

        sem_post(&mutex[segment]); // let other children of the same segment pass

        line = rand()%(semlock_info->lines-1); // get random line

        // get the new line as a string
        curLine = semlock_string;
        nextLine = strchr(curLine, '\n');
        for(i = 0; i < line; i++) {
            curLine = nextLine + 1;
            nextLine = strchr(curLine, '\n');
        }
        for(length = 0; *curLine != '\n' && *curLine != '\0'; length++) 
            curLine++;

        curLine -= length;

        strncpy(textLine, curLine, length);
        textLine[length] = '\0';

        gettimeofday(&current_time, NULL); // time of answer
        fprintf(fptr, "Time of answer:%ld\n", current_time.tv_usec);

        fprintf(fptr, "<%d, %d>\n", segment+1, line+1); // segment and line
        fprintf(fptr, "%s\n\n", textLine); // text of line

        usleep(20); // 20ms

        sem_wait(&mutex[segment]); // wait for children that chose the same segment
        semlock_count[segment]--; // counter used to manage the order of the children of the same segement
        if(semlock_count[segment] == 0) // last children of the segment
            sem_post(&semlock_info->rw_mutex); // let other segments pass
        sem_post(&mutex[segment]); // let other children of the same segment pass

        previous_segment = segment; 
        if(rand()%10 < 3) // segment change case
            for(segment = rand()%segments; segment == previous_segment; segment = rand()%segments);
    }
    /* end of communcation between child and parent */

    free(textLine); // free line

    fclose(fptr);

    /* detach memory segments */
    if(shmdt((void*)semlock_info) == -1) {
        perror("shmdt: shmdt for semlock_info failed");
        exit(1);
    }
    if(shmdt((void*)mutex) == -1) {
        perror("shmdt: shmdt for mutex failed");
        exit(1);
    }
    if(shmdt((void*)semlock_string) == -1) {
        perror("shmdt: shmdt for semlock_string failed");
        exit(1);
    }
    if(shmdt((void*)semlock_count) == -1) {
        perror("shmdt: shmdt for semlock_count failed");
        exit(1);
    }
}