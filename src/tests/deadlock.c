// Dla każdej maski (maski są wyjaśnione w utils.h) odpala wątki wykonujące
// losowe takie operacje.

#define THREAD_COUNT 8
#define OPERATIONS_IN_THREAD 5000

#include "utils.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>

typedef struct {
	Tree *tree;
	int mask;
	int initial_seed;
} ThreadData;

void* run_operations(void *data) {
	ThreadData *thread_data = (ThreadData*) data;

	for(int i = 0; i < OPERATIONS_IN_THREAD; ++i) {
		// printf("From %lu running operation %d\n", pthread_self(), i);
		Operation *operation = get_random_operation(&thread_data->initial_seed, thread_data->mask);
		run_operation(thread_data->tree, operation);
		free_operation(operation);
		// printf("From %lu finished operation %d\n", pthread_self(), i);
	}

	free(thread_data);
	return NULL;
}

void run_tests_for_mask(int mask) {
	assert(mask > 0);
	Tree *tree = tree_new();
	int seed = 0;
	run_some_creates(&seed, tree);

	pthread_attr_t attr;
	assert(pthread_attr_init(&attr) == 0);
	assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);

	pthread_t th[THREAD_COUNT];
	for(int i = 0; i < THREAD_COUNT; ++i) {
		ThreadData *data = malloc(sizeof(ThreadData));
		data->tree = tree;
		data->mask = mask;
		data->initial_seed = 100 + i;
		assert(pthread_create(&th[i], &attr, run_operations, data) == 0);
	}

	for(int i = 0; i < THREAD_COUNT; ++i) {
		void *retval;
		assert(pthread_join(th[i], &retval) == 0);
		assert(retval == NULL);
	}

	tree_free(tree);
}

void deadlock() {
	for(int mask = 1; mask < (1 << 4); ++mask) {
		printf("- running deadlock test for mask %d...\n", mask);
		run_tests_for_mask(mask);
	}
}
