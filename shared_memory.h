struct shared_memory {
    int segment;
    int lines;
    int requests;
    sem_t rw_mutex, requestConsumer, answerProducer;
};

typedef struct shared_memory* sharedMemory;