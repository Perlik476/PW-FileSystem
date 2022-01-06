// Komentarze są w utils.h.

#include "utils.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

void advance_seed(int *curr_seed) {
	*curr_seed = ((long long)(*curr_seed) * (*curr_seed) + 1) % (1000000007);
}

int rd(int *curr_seed, int l, int r) {
	advance_seed(curr_seed);
	return (*curr_seed) % (r - l + 1) + l;
}

char* get_random_small_path(int *curr_seed) {
	char *s = malloc(sizeof(char) * (2 + 4 * 3));
	s[0] = '/';
	s[1] = '\0';

	char *curr = s;
	int iter_cnt = rd(curr_seed, 0, 4);
	for(int iter = 0; iter < iter_cnt; ++iter) {
		assert(curr[0] == '/');
		int len = 1;
		for(int i = 0; i < len; ++i)
			curr[1 + i] = 'a' + rd(curr_seed, 0, 2);
		curr[1 + len] = '/';
		curr[1 + len + 1] = '\0';
		curr += 1 + len;
	}
	return s;
}

struct Operation {
 	int type;  
	char *path1, *path2;
};

Operation* construct_operation(int type, char *path1, char *path2) {
	assert(0 <= type && type < 4);	
	assert(path1 != NULL);
	if(type == 3)
		assert(path2 != NULL);
	else
		assert(path2 == NULL);
	Operation *operation = malloc(sizeof(Operation));
	operation->type = type;
	operation->path1 = path1;
	operation->path2 = path2;
	return operation;
}

Operation* get_random_operation(int *curr_seed, int mask) {
	assert(mask > 0 && mask < (1 << 4));

	Operation *operation = malloc(sizeof(Operation));
	do {
		operation->type = rd(curr_seed, 0, 3);
	} while(((1 << operation->type) & mask) == 0);

	operation->path1 = get_random_small_path(curr_seed);	
	if((1 << operation->type) == MASK_MOVE)
		operation->path2 = get_random_small_path(curr_seed);
	else
		operation->path2 = NULL;

	return operation;
}

void free_operation(Operation *operation) {
	free(operation->path1);
	if((1 << operation->type) == MASK_MOVE)
		free(operation->path2);
	free(operation);
}

int run_operation(Tree *tree, const Operation *operation) {
	if((1 << operation->type) == MASK_LIST) {
		char *str = tree_list(tree, operation->path1);
		free(str);
		return 0;
	}
	else if((1 << operation->type) == MASK_CREATE) {
		int err = tree_create(tree, operation->path1);
		assert(err == 0 || err == EINVAL || err == ENOENT || err == EEXIST);
		return err;
	}
	else if((1 << operation->type) == MASK_REMOVE) {
		int err = tree_remove(tree, operation->path1);
		assert(err == 0 || err == EINVAL || err == ENOENT || err == EBUSY || err == ENOTEMPTY);
		return err;
	}
	else {
		assert((1 << operation->type) == MASK_MOVE);
		int err = tree_move(tree, operation->path1, operation->path2);
		assert(err == 0 || err == EINVAL || err == ENOENT || err == EBUSY || err == EEXIST || (-20 <= err && err <= -1));
		return err;
	}
}

void run_some_creates(int *curr_seed, Tree *tree) {
	for(int i = 0; i < 100; ++i) {
		char *s = get_random_small_path(curr_seed);
		tree_create(tree, s);
		free(s);
	}
}

void print_operation(const Operation *operation) {
	if(operation->type == 0)
		printf("tree_list(tree, \"%s\")", operation->path1);
	else if(operation->type == 1)
		printf("tree_create(tree, \"%s\")", operation->path1);
	else if(operation->type == 2)
		printf("tree_remove(tree, \"%s\")", operation->path1);
	else if(operation->type == 3)
		printf("tree_move(tree, \"%s\", \"%s\")", operation->path1, operation->path2);
}

void print_ret_code(int err) {
#define RETURN_IF_MATCH(x, y) if(err == x) { printf(y); return; }
	RETURN_IF_MATCH(0, "0");
	RETURN_IF_MATCH(EINVAL, "EINVAL");
	RETURN_IF_MATCH(ENOENT, "ENOENT");
	RETURN_IF_MATCH(EBUSY, "EBUSY");
	RETURN_IF_MATCH(EEXIST, "EEXIST");
	RETURN_IF_MATCH(ENOTEMPTY, "ENOTEMPTY");
	if(-20 <= err && err <= -1) {
		printf("-1");
		return;
	}
	printf("%d", err);
	assert(false);
}

