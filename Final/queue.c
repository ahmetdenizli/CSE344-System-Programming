#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

/* Function to create a queue of given capacity. */
struct Queue* create_queue(int capacity)
{
    if (capacity == 0)
        capacity = 50;
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    queue->array = (int*)calloc(queue->capacity,sizeof(int));
    return queue;
}

/* Controls is queue empty */
int isEmpty(struct Queue* queue){
    return (queue->size == 0);
}

/* Controls is queue full */
int isFull(struct Queue* queue){
    return (queue->size == queue->capacity);
}

/* Function to add an item to the queue. */
int enqueue(struct Queue* queue, int item){
    if(isFull(queue)){
        queue->array=(int*)realloc(queue->array,(queue->capacity+500)*sizeof(int));
        queue->capacity+=500;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    ++queue->size;
    return queue->size;
}

/* Function to remove an item from queue. */
int dequeue(struct Queue* queue){
    if (queue->size == 0)
        return -1;

    int item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    --queue->size;
    return item;
}

/* Frees the queue structure */
void free_queue(struct Queue* queue){
    free(queue->array);
    free(queue);
}