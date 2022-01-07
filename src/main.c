#include "HashMap.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

//void print_map(HashMap* children) {
//    const char* key = NULL;
//    void* value = NULL;
//    printf("Size=%zd\n", hmap_size(children));
//    HashMapIterator it = hmap_iterator(children);
//    while (hmap_next(children, &it, &key, &value)) {
//        printf("Key=%s Value=%p\n", key, value);
//    }
//    printf("\n");
//}
//
//
//int main(void)
//{
//    HashMap* children = hmap_new();
//    hmap_insert(children, "a", hmap_new());
//    print_map(children);
//
//    HashMap* child = (HashMap*)hmap_get(children, "a");
//    hmap_free(child);
//    hmap_remove(children, "a");
//    print_map(children);
//
//    hmap_free(children);
//
//    return 0;
//}

#include "Tree.h"

int main() {
    Tree *tree = tree_new();

    printf("tree_new done\n");

    tree_create(tree, "/x/");

    printf("tree_create done\n");

    tree_create(tree, "/x/y/");

    printf("tree_create done\n");

    tree_create(tree, "/x/x/");

    printf("tree_create done\n");

    tree_create(tree, "/x/xdd/");

    printf("tree_create done\n");

    char *res = tree_list(tree, "/x/");
    printf("%s\n", res);
    free(res);

    printf("%d\n", tree_remove(tree, "/x/xdd/"));

    printf("tree_remove done\n");

    res = tree_list(tree, "/x/");
    printf("%s\n", res);
    free(res);

    tree_create(tree, "/x/xdd/");

    printf("tree_create done\n");

    tree_create(tree, "/lol/");

    printf("tree_create done\n");

    printf("%d\n", tree_move(tree, "/x/", "/x/"));

    printf("tree_move done\n");

    res = tree_list(tree, "/x/");
    printf("%s\n", res);
    free(res);

    res = tree_list(tree, "/lol/bro/");
    printf("%s\n", res);
    free(res);

    printf("%d", tree_create(tree, "/"));

    return 0;
}