#pragma once

#include <stddef.h>

typedef struct pqueuenode pqueuenode_t;
typedef struct pqueue pqueue_t;

pqueue_t *pqueue_create(size_t capacity);
void pqueue_free(pqueue_t *queue);
bool pqueue_enqueue(pqueue_t *queue, void *data, size_t priority);
void *pqueue_dequeue(pqueue_t *queue);

