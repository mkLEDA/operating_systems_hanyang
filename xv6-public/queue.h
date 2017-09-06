

typedef struct CircularQueue {
    void* key[64];
    int first;
    int rear;
    int qsize;
    int max_queue_size;
}CircularQueue;

void MakeEmpty(CircularQueue *newQueue);

void Enqueue (CircularQueue *Q, void* X);

void* Dequeue (CircularQueue *Q);

int Delete(CircularQueue *Q, void* X);
