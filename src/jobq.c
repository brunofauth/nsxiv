// vim: noexpandtab shiftwidth=8
#include "nsxiv.h"
#include "jobq.h"
#include <errno.h>
#include <stdlib.h>

// Debug {{{
#include <stdio.h>
#include <time.h>
// }}}

void *jobq_consume_msgs(void *pool_state) {
	jobq_pool_state_t *state = (jobq_pool_state_t*) pool_state;

	printf("inside consumer\n");
	struct timespec ts = {.tv_nsec = 200000000};
	nanosleep(&ts, NULL);

	while (state->keep_going) {
		job_t *msg = pqueue_dequeue(state->job_queue);
		switch (*msg) {
		case JOB_CACHE_IMG:
			break;
		case JOB_LOAD_HIDDEN_IMG:
			break;
		case JOB_LOAD_SHOWN_IMG:
			break;
		}
	}

	printf("leaving consumer\n");
	return NULL;
}

void jobq_pool_init(pthread_t threads[], size_t thread_count, jobq_pool_state_t *state) {
	int status;
	size_t thread_errors = 0;

	for (size_t i = 0; i < thread_count; i++) {
		status = pthread_create(&threads[i], NULL, jobq_consume_msgs, &state);

		switch (status) {
		case 0: continue;
		case EAGAIN: thread_errors += 1; break;
		default: error(EXIT_FAILURE, status, "could not create thread pool"); break;
		}
	}

	if (thread_errors == thread_count)
		error(EXIT_FAILURE, EAGAIN, "could not create any threads");
	if (thread_errors) {
		// TODO: log to the console that we couldn't start <(n - errors)> threads
	}
}


