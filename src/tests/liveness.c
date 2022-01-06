// W wielu wątkach w while'u jest odpalany create albo list,
// a w jednym oddzielnym wątku jest odpalana przeciwna operacja
// (create albo list) ze sleep'ami.

#define THREAD_COUNT 8
#define SLEEP_SECONDS 0.1

#include "utils.h"

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct {
	Tree *tree;
	bool is_create;
	int initial_seed;
} ThreadData;

bool run;

char* get_root() {
	char *path = malloc(sizeof(char) * 2);
	path[0] = '/';
	path[1] = '\0';
	return path;
}

char *get_long_folder(int *curr_seed) {
	const int MAX_FOLDER_NAME_LENGTH = 255;
	char *path = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 3));
	path[0] = '/';
	for(int i = 0; i < MAX_FOLDER_NAME_LENGTH; ++i)
		path[1 + i] = 'a' + rd(curr_seed, 0, 26 - 1);
	path[1 + MAX_FOLDER_NAME_LENGTH] = '/';
	path[1 + MAX_FOLDER_NAME_LENGTH + 1] = '\0';
	return path;
}

Operation *get_create_or_list(int *curr_seed, bool is_create) {
	return construct_operation(is_create, is_create ? get_long_folder(curr_seed) : get_root(), NULL);
}

void* run_operations_in_loop(void *data) {
	ThreadData *thread_data = (ThreadData*) data;

	while(run) {
		Operation *operation = get_create_or_list(&thread_data->initial_seed, thread_data->is_create);
		run_operation(thread_data->tree, operation);
		free_operation(operation);
	}

	free(thread_data);
	return NULL;
}

void *run_operations_individual(void *data) {
	ThreadData *thread_data = (ThreadData*) data;

	for(int i = 0; i < (0.5 / SLEEP_SECONDS); ++i) {
		nanosleep((const struct timespec[]){{0, SLEEP_SECONDS * 1000000000L}}, NULL);
		Operation *operation = get_create_or_list(&thread_data->initial_seed, thread_data->is_create);
		run_operation(thread_data->tree, operation);
		free_operation(operation);
	}

	free(thread_data);
	return NULL;
}

void liveness() {
	for(int create_in_loop = 0; create_in_loop <= 1; ++create_in_loop) {
		printf("- running longevity test %d...\n", create_in_loop);

		pthread_attr_t attr;
		assert(pthread_attr_init(&attr) == 0);
		assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);

		Tree *tree = tree_new();
		pthread_t th[THREAD_COUNT];
		run = true;
		for(int i = 0; i < THREAD_COUNT; ++i) {
			ThreadData *data = malloc(sizeof(ThreadData));
			data->tree = tree;
			data->is_create = (i == 0 ? !create_in_loop : create_in_loop);
			data->initial_seed = 100 + i;
			assert(pthread_create(&th[i], &attr, (i == 0 ? run_operations_individual : run_operations_in_loop), data) == 0);
		}

		void *retval;
		assert(pthread_join(th[0], &retval) == 0);
		assert(retval == NULL);

		run = false;
		for(int i = 1; i < THREAD_COUNT; ++i) {
			assert(pthread_join(th[i], &retval) == 0);
			assert(retval == NULL);
		}

		tree_free(tree);
	}
}
