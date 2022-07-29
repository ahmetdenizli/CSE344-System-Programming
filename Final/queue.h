
#ifndef INC_344FINAL_QUEUE_H
#define INC_344FINAL_QUEUE_H

struct Queue
{
    int front;
    int rear;
    int size;
    int capacity;
    int *array;
};

/* functions */
/* Function to create a queue of given capacity. */
struct Queue* create_queue(int capacity);

/* Controls is queue full */
int isFull(struct Queue* queue);

/* Controls is queue empty */
int isEmpty(struct Queue* queue);

/* Function to add an item to the queue. */
int enqueue(struct Queue* queue, int item);

/* Function to remove an item from queue. */
int dequeue(struct Queue* queue);

/* Frees the queue structure */
void free_queue(struct Queue* queue);

#endif //INC_344FINAL_QUEUE_H
