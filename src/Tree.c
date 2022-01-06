#include <errno.h>
#include <stdlib.h>

#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include "string.h"
#include "stdio.h"

struct Node {
    HashMap *map;
};

typedef struct Node Node;

struct Tree {
    Node *root;
};

Node *node_new() {
    Node *node = malloc(sizeof(Node));
    node->map = hmap_new();
    return node;
}

void node_destroy(Node *node) {
    const char *key = NULL;
    void *value = NULL;
    HashMapIterator it = hmap_iterator(node->map);
    while (hmap_next(node->map, &it, &key, &value)) {
        node_destroy((Node *) value);
        free((char *) key);
    }
    hmap_free(node->map);
    free(node);
}

Node *get_node(Node *node, const char *path) {
//    printf("%s\n", path);
    if (!strcmp(path, "/")) {
        return node;
    }

    char *next_node_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    const char *new_path = split_path(path, next_node_name);

    Node *next_node = hmap_get(node->map, next_node_name);
    free(next_node_name);

    Node *result_node = get_node(next_node, new_path);
    free(next_node_name);
    return result_node;
}

int add_child(Node *parent, Node *child, const char *child_name) {
    if (hmap_get(parent->map, child_name)) {
        return EEXIST;
    }

    hmap_insert(parent->map, child_name, child);

    return 0;
}

int remove_child(Node *parent, const char *child_name) {
    Node *node = hmap_get(parent->map, child_name);

    if (!node) {
        return ENOENT;
    }
    if (hmap_size(node->map)) {
        return ENOTEMPTY;
    }

    hmap_remove(parent->map, child_name);
    node_destroy(node);

    return 0;
}

char *get_children_names(Node *node) {
//    printf("xd\n");
    return make_map_contents_string(node->map);
}

Tree *tree_new() {
    Tree *tree = malloc(sizeof(Tree));
    tree->root = node_new();
    return tree;
}

void tree_free(Tree *tree) {
    node_destroy(tree->root);
    free(tree);
}

char *tree_list(Tree *tree, const char *path) {
//    printf("tree_list\n");
    if (!is_path_valid(path)) {
        return NULL;
    }

    Node *node = get_node(tree->root, path);
    if (!node) {
        return NULL;
    }

    return get_children_names(node);
}

int tree_create(Tree *tree, const char *path) {
//    printf("tree_create\n");
    if (!is_path_valid(path)) {
        return EINVAL;
    }

    char *new_node_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_parent = make_path_to_parent(path, new_node_name);

    Node *parent = get_node(tree->root, path_to_parent);
    if (!parent) {
        return ENOENT;
    }

    Node *new_node = node_new();
    int err = add_child(parent, new_node, new_node_name);
//    printf("tree_create: %d\n", err);
    return err;
}

int tree_remove(Tree *tree, const char *path) {
//    printf("tree_remove\n");
    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        return EBUSY;
    }

    char *child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_parent = make_path_to_parent(path, child_name);

    Node *parent = get_node(tree->root, path_to_parent);
    if (!parent) {
        return ENOENT;
    }

    int err = remove_child(parent, child_name);
    free(child_name);
//    printf("tree_remove: %d\n", err);
    return err;
}

int tree_move(Tree *tree, const char *source, const char *target) {
//    printf("tree_move\n");
    if (!is_path_valid(source) || !is_path_valid(target)) {
        return EINVAL;
    }

    char *source_child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_source_parent = make_path_to_parent(source, source_child_name);

    Node *source_parent_node = get_node(tree->root, path_to_source_parent);
    if (!source_parent_node) {
        return ENOENT;
    }

//    printf("xd1\n");

    Node *source_node = (Node *)hmap_get(source_parent_node->map, source_child_name);
    if (!source_node) {
        return ENOENT;
    }

//    printf("xd2\n");

    char *target_child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_target_parent = make_path_to_parent(target, target_child_name);

    Node *target_parent_node = get_node(tree->root, path_to_target_parent);
    if (!target_parent_node) {
        return ENOENT;
    }

//    printf("xd3\n");

    hmap_remove(source_parent_node->map, source_child_name);

//    printf("xd4\n");

    int err = add_child(target_parent_node, source_node, target_child_name);

    free(path_to_source_parent);
    free(path_to_target_parent);
    free(source_child_name);
//    printf("tree_move: %d\n", err);
    return err;
}
