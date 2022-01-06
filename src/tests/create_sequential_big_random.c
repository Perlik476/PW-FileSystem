#define OPERATION_COUNT 10000

#include "utils.h"

#include <stdio.h>

int main() {
	printf("#include \"sequential_big_random.h\"\n");
	printf("#include \"../Tree.h\"\n");
	printf("#include <assert.h>\n");
	printf("#include <errno.h>\n");
	printf("\n");
	printf("int f(int err) { if (-20 <= err && err <= -1) return -1; return err; }\n");
	printf("void sequential_big_random() {\n");
	printf("	Tree *tree = tree_new();\n");
	Tree *tree = tree_new();
	int curr_seed = 0;
	for(int i = 0; i < OPERATION_COUNT; ++i) {
		Operation *operation = get_random_operation(&curr_seed, MASK_CREATE | MASK_REMOVE | MASK_MOVE);
		int err = run_operation(tree, operation);
		if(-20 <= err && err <= -1)
			err = -1;
		printf("	assert(f(");
		print_operation(operation);
		printf(") == ");
		print_ret_code(err);
		printf(");\n");
		free_operation(operation);
	}
	printf("	tree_free(tree);\n");
	printf("}\n");
}
