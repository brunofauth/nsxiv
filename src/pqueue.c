#include "nsxiv.h"
#include "pqueue.h"
#include <stdbool.h>
#include <pthread.h>


struct pqueuenode {
	void *data;
	int priority;
};

struct pqueue {
	size_t size;
	size_t capacity;
	pthread_mutex_t mutex;
	pthread_cond_t has_items;
	pqueuenode_t heap[];
};

pqueue_t *pqueue_create(size_t capacity) {
	pqueue_t *queue = malloc(sizeof(pqueue_t) + capacity * sizeof(pqueuenode_t));
	// From the man pages: pthread_mutex_init always returns 0
	pthread_mutex_init(&queue->mutex, NULL);
	queue->capacity = capacity;
	queue->size = 0;
	// From the man pages: pthread_cond_init never returns error codes
	pthread_cond_init(&queue->has_items, NULL);
	return queue;
}

void pqueue_free(pqueue_t *queue) {
	int status;
	if ((status = pthread_mutex_destroy(&queue->mutex)))
		error(EXIT_FAILURE, status, "could not free queue (mutex busy)");
	if ((status = pthread_cond_destroy(&queue->has_items)))
		error(EXIT_FAILURE, status, "could not free queue (has_items busy)");
	free(queue);
}

static void swap(pqueuenode_t *a, pqueuenode_t *b) {
	pqueuenode_t temp = *a;
	*a = *b;
	*b = temp;
}

static void heapify_up(pqueuenode_t *heap, size_t index) {
	int parent = (index - 1) / 2;
	while (index > 0 && heap[parent].priority < heap[index].priority) {
		swap(&heap[parent], &heap[index]);
		index = parent;
		parent = (index - 1) / 2;
	}
}

bool pqueue_enqueue(pqueue_t *queue, void *data, size_t priority) {
	int mutex_status;
	if ((mutex_status = pthread_mutex_lock(&queue->mutex)))
		error(EXIT_FAILURE, mutex_status, "could not enqueue (deadlock)");
	if (queue->size == queue->capacity) {
		if ((mutex_status = pthread_mutex_unlock(&queue->mutex)))
			error(EXIT_FAILURE, mutex_status, "could not enqueue (mutex not owned)");
		return false;
	}

	pqueuenode_t newNode;
	newNode.data = data;
	newNode.priority = priority;
	queue->heap[queue->size] = newNode;
	heapify_up(queue->heap, queue->size);
	queue->size++;

	// From the man pages: pthread_cond_signal never returns error codes
	pthread_cond_signal(&queue->has_items);

	if ((mutex_status = pthread_mutex_unlock(&queue->mutex)))
		error(EXIT_FAILURE, mutex_status, "could not enqueue (mutex not owned)");
	return true;
}

static void heapify_down(pqueuenode_t *heap, size_t index, ssize_t heap_size) {
	int leftChild = 2 * index + 1;
	int rightChild = 2 * index + 2;
	int largest = index;

	if (leftChild < heap_size &&
			heap[leftChild].priority > heap[largest].priority) {
		largest = leftChild;
	}

	if (rightChild < heap_size &&
			heap[rightChild].priority > heap[largest].priority) {
		largest = rightChild;
	}

	if (largest != index) {
		swap(&heap[index], &heap[largest]);
		heapify_down(heap, largest, heap_size);
	}
}

void *pqueue_dequeue(pqueue_t *queue) {
	int mutex_status;
	if ((mutex_status = pthread_mutex_lock(&queue->mutex)))
		error(EXIT_FAILURE, mutex_status, "could not dequeue (deadlock)");
	if (queue->size == 0)
		// From the man pages: pthread_cond_wait never returns error codes
		pthread_cond_wait(&queue->has_items, &queue->mutex);

	void *result = queue->heap[0].data;
	queue->heap[0] = queue->heap[queue->size - 1];
	queue->size--;
	heapify_down(queue->heap, 0, queue->size);

	if ((mutex_status = pthread_mutex_unlock(&queue->mutex)))
		error(EXIT_FAILURE, mutex_status, "could not dequeue (mutex not owned)");
	return result;
}
