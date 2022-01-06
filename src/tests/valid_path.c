// Test sprawdzający, czy dobrze się sprawdza poprawność ścieżki.
// Test zakłada, że tree_create na samym początku sprawdza, czy string jest poprawny.

#include "../Tree.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>

const int MAX_PATH_LENGTH = 4095;
const int MAX_FOLDER_NAME_LENGTH = 255;

bool my_path_valid(const char *path) {
	Tree *tree = tree_new();	
	int ret = tree_create(tree, path);
	tree_free(tree);
	return ret != EINVAL;
}

char* fill_with_component(char *s, size_t len) {
	s[0] = '/';
	for (size_t i = 1; i <= len; ++i)
		s[i] = 'a';
	s[len + 1] = '/';
	s[len + 2] = '\0';
	return s + len + 1;
}

void valid_path() {
	assert(my_path_valid("/"));
	assert(my_path_valid("/a/"));
	assert(my_path_valid("/a/b/"));
	assert(my_path_valid("/ab/bc/"));
	assert(my_path_valid("/a/bb/ccc/dddd/eeeee/"));
	assert(!my_path_valid(""));
	assert(!my_path_valid("//"));
	assert(!my_path_valid("/a//"));
	assert(!my_path_valid("/_/"));

	char s[MAX_PATH_LENGTH + 2];

	fill_with_component(s, MAX_FOLDER_NAME_LENGTH);
	assert(my_path_valid(s));

	fill_with_component(s, MAX_FOLDER_NAME_LENGTH + 1);
	assert(!my_path_valid(s));

	char *curr_start = s;
	while ((curr_start - s) + (MAX_FOLDER_NAME_LENGTH + 1) < MAX_PATH_LENGTH)
		curr_start = fill_with_component(curr_start, MAX_FOLDER_NAME_LENGTH);

	fill_with_component(curr_start, MAX_PATH_LENGTH - (curr_start - s) - 2);
	assert(my_path_valid(s));

	fill_with_component(curr_start, MAX_PATH_LENGTH - (curr_start - s) - 2 + 1);
	assert(!my_path_valid(s));
}
