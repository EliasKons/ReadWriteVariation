#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include "shared_memory.h"

#define MAXLENGTH 1000 
#define SEGMENTPERM 0666

void child(int requests, int segments, int n, int max_length, sharedMemory semlock_info, char* semlock_string, int* semlock_count, sem_t *mutex);

int main(int argc, char *argv[]) {
    int i, j, length, *semlock_count, *read_count, max_length = 0, lines = 0, request_count = 0;
    static int segments, n, requests, segment_size, shmid_info, shmid_string, shmid_count, shmid_mutex, total_requests, over;
    char *ptr, line[MAXLENGTH], **text, *semlock_string, *temp, **name;
    pid_t c;
    FILE *fptr;
    sem_t *mutex;
    sharedMemory semlock_info;
    

    // check for arguments given by the user
    if(argc < 4) {
        printf("Invalid arguments\n");
        return 1;
    }

    ptr = argv[1]; // file given by user
    segments = atoi(argv[2]); // number of segments 
    n = atoi(argv[3]); // number of child processes 
    requests = atoi(argv[4]); // number of requests per child  
    fptr = fopen(ptr, "r");

    /* calculate number of lines and maximum length of every line */
    while(fgets(line, MAXLENGTH, fptr) != NULL) {
        length = strlen(line);
        if (max_length < length)
            max_length = length;
        lines++;
    }
    max_length++;
    segment_size = lines/segments; // size of each segment

    /* segment memory initialization */
    if((shmid_info = shmget(IPC_PRIVATE, sizeof(sharedMemory), SEGMENTPERM)) == -1) {
        perror("shmget: shmget for shmid_info failed");
        exit(1);
    }
    if((shmid_mutex = shmget(IPC_PRIVATE, segments * sizeof(sem_t), SEGMENTPERM)) == -1) {
        perror("shmget: shmget for shmid_mutex failed");
        exit(1);  
    }
    if((shmid_string = shmget(IPC_PRIVATE, segment_size * max_length * sizeof(char), SEGMENTPERM)) == -1) {
        perror("shmget: shmget for shmid_string failed");
        exit(1);  
    }
    if((shmid_count = shmget(IPC_PRIVATE, segments * sizeof(int), SEGMENTPERM)) == -1) {
        perror("shmget: shmget for shmid_count failed");
        exit(1);  
    }


    /* memory attachment */
    if((semlock_info = (sharedMemory)shmat(shmid_info, NULL, 0)) == (void*) -1) {
        perror("shmat: shmat for semlock_info failed");
        exit(1);
    }
    if((mutex = (sem_t*)shmat(shmid_mutex, NULL, 0)) == (void*) -1) {
        perror("shmat: shmat for mutex failed");
        exit(1);
    }
    if((semlock_string = (char*)shmat(shmid_string, NULL, 0)) == (void*) -1) {
        perror("shmat: shmat for semlock_string failed");
        exit(1);
    }
    if((semlock_count = (int*)shmat(shmid_count, NULL, 0)) == (void*)-1) {
        perror("shmat: shmat semlock_count failed");
        exit(1);
    }

    /* initialize mutex and count array */
    for(i = 0; i < segments; i++) {
        sem_init(&mutex[i], 1, 1);
        if(&mutex[i] == (void*) -1) {
            perror("sem_init: sem_init for mutex failed");
            exit(1);
        }
        semlock_count[i] = 0;      
    }
    semlock_info->requests = 0; // requests done so far
    

    /* semaphores initialization */
    sem_init(&semlock_info->rw_mutex, 1, 1);
    sem_init(&semlock_info->requestConsumer, 1, 0);
    sem_init(&semlock_info->answerProducer, 1, 0);

    if(&semlock_info->rw_mutex == (void*) -1) {
            perror("sem_init: sem_init for rw_mutex failed");
            exit(1);
    }
    if(&semlock_info->requestConsumer == (void*) -1) {
            perror("sem_init: sem_init for requestConsumer failed");
            exit(1);
    }
    if(&semlock_info->answerProducer == (void*) -1) {
            perror("sem_init: sem_init for answerProducer failed");
            exit(1);
    }

    fclose(fptr);

    /* children creation */
    for(i = 0; i < n; i++) {
        c = fork();
        if(c < 0) {
            perror("fork has failed");
            exit(1);
        }
        else if(c == 0) {
            child(requests, segments, n, max_length, semlock_info, semlock_string, semlock_count, mutex); // child function call
            exit(0); // terminate
        }
    }

    fptr = fopen(ptr, "r");
    text = malloc(segments * sizeof(char*)); // segments initialization
    for(i = 0; i < segments-1; i++) { 
        text[i] = malloc(segment_size * max_length * sizeof(char)); 
        text[i][0] = '\0';

        /* segment creation */
        for(j = 0; j < segment_size; j++) 
            strcat(text[i], fgets(line, max_length, fptr));
    }
    // last segment takes the rest of the lines, if any
    over = lines%segments;
    text[i] = malloc((segment_size + over) * max_length * sizeof(char));  
    text[i][0] = '\0';

    /* segment creation */
    for(j = 0; j < segment_size + over; j++) 
        strcat(text[i], fgets(line, max_length, fptr)); 

    fclose(fptr);

    total_requests = n*requests; // total requests allowed

    /* start of communication between parent and children */
    while(1) {
        sem_wait(&semlock_info->requestConsumer); // wait request from consumer

        if(semlock_info->requests == total_requests) // special case where parent has to stop
            break;

        strcpy(semlock_string, text[semlock_info->segment]); // pass segment to shared memory

        // pass number of lines to shared memory
        if(semlock_info->segment != segments - 1)
            semlock_info->lines = segment_size;
        else
            semlock_info->lines = segment_size + over;

        sem_post(&semlock_info->answerProducer); // answer from producer
    }
    /* end of communication between parent and children */

    // free segments
    for(i = 0; i < segments; i++)
        free(text[i]);
    free(text);

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

    /* destroy memory segments */
    if(shmctl(shmid_info, IPC_RMID, 0) == -1) {
        perror("shmctl: shmctl for shmid_info failed");
        exit(1);
    }
    if(shmctl(shmid_mutex, IPC_RMID, 0) == -1) {
        perror("shmctl: shmctl for shmid_mutex failed");
        exit(1);
    }
    if(shmctl(shmid_string, IPC_RMID, 0) == -1) {
        perror("shmctl: shmctl for shmid_string failed");
        exit(1);
    }
    if(shmctl(shmid_count, IPC_RMID, 0) == -1) {
        perror("shmctl: shmctl for shmid_count failed");
        exit(1);
    }

    return 0;
}