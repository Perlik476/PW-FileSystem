// Odpala THREAD_COUNT wątków, każdy wykonuje jedną wylosowaną operację.
// Następnie sprawdza, czy na pewno wśród THREAD_COUNT! permutacji
// operacji istnieje taka permutacja, że jej sekwencyjne wykonanie
// daje takie same kody błędów.

#define THREAD_COUNT 6
#define ITERATIONS 10000

#include "utils.h"
#include "concurrent_same_as_some_sequential.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
	Tree *tree;
	Operation *operation;
	int ret_err;
} ThreadData;

void* run_operation_in_thread(void *data) {
	ThreadData *thread_data = data;
	thread_data->ret_err = run_operation(thread_data->tree, thread_data->operation);
	return NULL;
}

void run_sequentially(const ThreadData operations[], int curr_perm[], int sequential_answers[]) {
	Tree *tree = tree_new();
	for(int i = 0; i < THREAD_COUNT; ++i) {
		int j = curr_perm[i];
		sequential_answers[j] = run_operation(tree, operations[j].operation);
	}
	tree_free(tree);
}

bool backtrack(int curr_perm[], bool is_taken[], const ThreadData concurrent_answers[]) {
	bool is_whole_permutation = true;
	for(int i = 0; i < THREAD_COUNT; ++i)
		if(!is_taken[i]) {
			is_whole_permutation = false;
			break;
		}

	if(is_whole_permutation) {
		int sequential_answers[THREAD_COUNT];
		run_sequentially(concurrent_answers, curr_perm, sequential_answers);

		bool correct = true;
		for(int i = 0; i < THREAD_COUNT; ++i)
			if(sequential_answers[i] != concurrent_answers[i].ret_err)
				correct = false;
		return correct;
	}

	int i = 0;
	while(curr_perm[i] != -1)
		++i;
	assert(i < THREAD_COUNT);

	for(int j = 0; j < THREAD_COUNT; ++j)
		if(!is_taken[j]) {
			is_taken[j] = true;
			curr_perm[i] = j;
			if(backtrack(curr_perm, is_taken, concurrent_answers))
				return true;
			curr_perm[i] = -1;
			is_taken[j] = false;
		}
	return false;
}

bool exists_permutation(const ThreadData concurrent_answers[]) {
	int curr_perm[THREAD_COUNT];
	bool is_taken[THREAD_COUNT];
	for(int i = 0; i < THREAD_COUNT; ++i) {
		curr_perm[i] = -1;
		is_taken[i] = false;
	}
	return backtrack(curr_perm, is_taken, concurrent_answers);
}

void concurrent_same_as_some_sequential() {
	pthread_attr_t attr;
	assert(pthread_attr_init(&attr) == 0);
	assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);

	for(int iteration = 0; iteration < ITERATIONS; ++iteration) {
		int curr_seed = iteration;
		Tree *tree = tree_new();

		ThreadData thread_data[THREAD_COUNT];
		for(int i = 0; i < THREAD_COUNT; ++i) {
			thread_data[i].tree = tree;
			thread_data[i].operation = get_random_operation(&curr_seed, MASK_ALL);
			thread_data[i].ret_err = 0;
		}

		pthread_t th[THREAD_COUNT];
		for(int i = 0; i < THREAD_COUNT; ++i)
			assert(pthread_create(&th[i], &attr, run_operation_in_thread, &thread_data[i]) == 0);

		for(int i = 0; i < THREAD_COUNT; ++i) {
			void *retval;
			assert(pthread_join(th[i], &retval) == 0);
			assert(retval == NULL);
		}
		tree_free(tree);

		assert(exists_permutation(thread_data));

		for(int i = 0; i < THREAD_COUNT; ++i)
			free_operation(thread_data[i].operation);
	}
}
