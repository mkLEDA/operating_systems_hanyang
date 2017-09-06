#include "queue.h"

void MakeEmpty(CircularQueue* newQueue) {
    newQueue->first = 0;
    newQueue->rear = -1;
    newQueue->qsize = 0;
    newQueue->max_queue_size = 64;
    int i = 0;
    for(i = 0; i < 64; i ++) {
        newQueue->key[i] = 0;
    }
}


void Enqueue (CircularQueue* Q, void* X) {
    if (Q->qsize < Q->max_queue_size) {
        Q->qsize++;
        Q->rear = (Q->rear + 1)%(Q->max_queue_size);
        Q->key[Q->rear] = X;
    }
}

void* Dequeue (CircularQueue* Q) {
    if (Q->qsize > 0) {
        Q->qsize--;
        void* X = Q->key[Q->first];
        Q->first = (Q->first + 1) % (Q->max_queue_size);
        return X;
    }
    return 0;
}

int Delete(CircularQueue* Q, void* X) {
   if(Q) {
       int i = 0;
       while(i < 64 && Q->key[i] != X) {
            i++;
       }
       if(i < 64) {
            Q->key[i] = 0;
            return 1;
       }
   }
   return 0;

}
