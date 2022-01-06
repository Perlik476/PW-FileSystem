#include <errno.h>
#include <stdlib.h>

#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include "string.h"
#include "stdio.h"
#include "err.h"
#include <pthread.h>

struct Node {
    HashMap *map;
};

typedef struct Node Node;

struct Tree {
    Node *root;
    pthread_mutex_t mutex;
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

    if (!next_node) {
        return NULL;
    }

    Node *result_node = get_node(next_node, new_path);
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
    pthread_mutex_lock(&tree->mutex);

    if (!is_path_valid(path)) {
        pthread_mutex_unlock(&tree->mutex);
        return NULL;
    }

    Node *node = get_node(tree->root, path);
    if (!node) {
        pthread_mutex_unlock(&tree->mutex);
        return NULL;
    }



    char *result = get_children_names(node);

    pthread_mutex_unlock(&tree->mutex);

    return result;
}

int tree_create(Tree *tree, const char *path) {
//    printf("tree_create\n");
    pthread_mutex_lock(&tree->mutex);
    if (!is_path_valid(path)) {
        pthread_mutex_unlock(&tree->mutex);
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        pthread_mutex_unlock(&tree->mutex);
        return EEXIST;
    }

    char *new_node_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_parent = make_path_to_parent(path, new_node_name);

    Node *parent = get_node(tree->root, path_to_parent);
    free(path_to_parent);
    if (!parent) {
        free(new_node_name);
        pthread_mutex_unlock(&tree->mutex);
        return ENOENT;
    }

    Node *new_node = node_new();
    int err = add_child(parent, new_node, new_node_name);
    free(new_node_name); // TODO ????
    if (err != 0) {
        node_destroy(new_node);
    }
//    printf("tree_create: %d\n", err);
    pthread_mutex_unlock(&tree->mutex);
    return err;
}

int tree_remove(Tree *tree, const char *path) {
//    printf("tree_remove\n");
    pthread_mutex_lock(&tree->mutex);
    if (!is_path_valid(path)) {
        pthread_mutex_unlock(&tree->mutex);
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        pthread_mutex_unlock(&tree->mutex);
        return EBUSY;
    }

    char *child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_parent = make_path_to_parent(path, child_name);

    Node *parent = get_node(tree->root, path_to_parent);
    if (!parent) {
        free(child_name);
        free(path_to_parent);
        pthread_mutex_unlock(&tree->mutex);
        return ENOENT;
    }

    int err = remove_child(parent, child_name);
    free(child_name);
    free(path_to_parent);
//    printf("tree_remove: %d\n", err);
    pthread_mutex_unlock(&tree->mutex);
    return err;
}

void print_map(HashMap* map) {
    const char* key = NULL;
    void* value = NULL;
    printf("Size=%zd\n", hmap_size(map));
    HashMapIterator it = hmap_iterator(map);
    while (hmap_next(map, &it, &key, &value)) {
        printf("Key=%s Value=%p\n", key, value);
    }
    printf("\n");
}

int tree_move(Tree *tree, const char *source, const char *target) {
    pthread_mutex_lock(&tree->mutex);
//    printf("tree_move\n");
    if (!is_path_valid(source) || !is_path_valid(target)) {
        pthread_mutex_unlock(&tree->mutex);
        return EINVAL;
    }
    if (!strcmp(source, "/")) {
        pthread_mutex_unlock(&tree->mutex);
        return EBUSY;
    }
    if (!strcmp(target, "/")) {
        pthread_mutex_unlock(&tree->mutex);
        return EEXIST;
    }

    if (is_substring(source, target)) {
//        free(source_child_name);
//        free(path_to_source_parent);
//        free(target_child_name);
//        free(path_to_target_parent);
        pthread_mutex_unlock(&tree->mutex);
        return -1;
    }


    char *source_child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_source_parent = make_path_to_parent(source, source_child_name);

    Node *source_parent_node = get_node(tree->root, path_to_source_parent);
    if (!source_parent_node) {
        free(source_child_name);
        free(path_to_source_parent);
        pthread_mutex_unlock(&tree->mutex);
        return ENOENT;
    }



    Node *source_node = (Node *)hmap_get(source_parent_node->map, source_child_name);
    if (!source_node) {
//        print_map(tree->root->map);
//        printf("%s, %s\n", path_to_source_parent, source_child_name);
        free(source_child_name);
        free(path_to_source_parent);
        pthread_mutex_unlock(&tree->mutex);
        return ENOENT;
    }

    char *target_child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_target_parent = make_path_to_parent(target, target_child_name);

    Node *target_parent_node = get_node(tree->root, path_to_target_parent);

    if (!target_parent_node) {
        free(source_child_name);
        free(path_to_source_parent);
        free(target_child_name);
        free(path_to_target_parent);
        pthread_mutex_unlock(&tree->mutex);
        return ENOENT;
    }

    if (!strcmp(source, target)) {
        free(path_to_source_parent);
        free(path_to_target_parent);
        free(source_child_name);
        free(target_child_name);
        pthread_mutex_unlock(&tree->mutex);
        return 0;
    }

//    if (target_parent_node == source_node) {
//        return -1;
//    }


    int err = add_child(target_parent_node, source_node, target_child_name);

    if (!err) {
        hmap_remove(source_parent_node->map, source_child_name);
    }

    free(path_to_source_parent);
    free(path_to_target_parent);
    free(source_child_name);
    free(target_child_name);
//    printf("tree_move: %d\n", err);
    pthread_mutex_unlock(&tree->mutex);
    return err;
}
