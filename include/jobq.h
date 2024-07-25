// vim: noexpandtab shiftwidth=8
#pragma once

#include <stdatomic.h>
#include <pthread.h>
#include "pqueue.h"

typedef enum {
	JOB_CACHE_IMG = 10,
       	JOB_LOAD_HIDDEN_IMG = 20,
       	JOB_LOAD_SHOWN_IMG = 30
} job_t;

typedef struct {
	atomic_bool keep_going;
	pqueue_t *job_queue;
} jobq_pool_state_t;


void *jobq_consume_msgs(void *pool_state);
void jobq_pool_init(pthread_t threads[], size_t thread_count, jobq_pool_state_t *state);
