#include <errno.h>
#include <stdlib.h>

#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include "string.h"
#include "stdio.h"
#include "err.h"
#include <pthread.h>
#include <assert.h>

#define READER_BEGIN 1
#define READER_END 2
#define WRITER_BEGIN 3
#define WRITER_END 4


typedef struct Node Node;

struct Node {
    HashMap *children;

    pthread_mutex_t mutex;
    pthread_cond_t writers;
    pthread_cond_t readers;
    int readers_count, readers_wait, writers_count, writers_wait, how_many_moving;
    bool can_readers_enter, is_removed;
    Node *parent;
    char *name;
};

struct Tree {
    Node *root;
};

Node *node_new() {
    Node *node = malloc(sizeof(Node));
    node->children = hmap_new();

    int err;
    if ((err = pthread_mutex_init(&node->mutex, 0)) != 0) {
        syserr(err, "mutex init failed");
    }
    if ((err = pthread_cond_init(&node->readers, 0)) != 0) {
        syserr(err, "cond readers init failed");
    }
    if ((err = pthread_cond_init(&node->writers, 0)) != 0) {
        syserr(err, "cond writers init failed");
    }

    node->readers_count = 0;
    node->writers_count = 0;
    node->readers_wait = 0;
    node->writers_wait = 0;
    node->can_readers_enter = true;
    node->how_many_moving = 0;
    node->is_removed = false;

    return node;
}

void node_destroy(Node *node) {
    const char *key = NULL;
    void *value = NULL;
    HashMapIterator it = hmap_iterator(node->children);
    while (hmap_next(node->children, &it, &key, &value)) {
        node_destroy((Node *) value);
    }
    hmap_free(node->children);

    int err;
    if ((err = pthread_cond_destroy(&node->readers)) != 0) {
        syserr(err, "cond readers destroy failed");
    }
    if ((err = pthread_cond_destroy(&node->writers)) != 0) {
        syserr(err, "cond writers destroy failed");
    }
    if ((err = pthread_mutex_destroy(&node->mutex)) != 0) {
        syserr(err, "mutex destroy failed");
    }

    free(node);
}



void reader_beginning_protocol(Node *node) {
    int err;
    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    if (!node->can_readers_enter) {
        node->readers_wait++;
        while (!node->can_readers_enter) {
            if ((err = pthread_cond_wait(&node->readers, &node->mutex)) != 0) {
                syserr(err, "cond readers wait failed");
            }
        }
        node->readers_wait--;
    }

    node->readers_count++;
    if (node->readers_wait == 0 && node->writers_wait > 0) {
        node->can_readers_enter = 0;
    }

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}

void reader_ending_protocol(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->readers_count--;
    if (node->readers_count == 0 && node->writers_wait > 0) {
        node->can_readers_enter = 0;
        if ((err = pthread_cond_broadcast(&node->writers)) != 0) {
            syserr(err, "cond writers broadcast failed");
        }
    }

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}


void writer_beginning_protocol(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->can_readers_enter = 0;

    if (node->readers_count + node->writers_count > 0) {
        node->writers_wait++;
        while (node->readers_count + node->writers_count > 0) {
            if ((err = pthread_cond_wait(&node->writers, &node->mutex)) != 0) {
                syserr(err, "cond writers wait failed");
            }
        }
        node->writers_wait--;
    }

    node->writers_count++;

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}

void writer_ending_protocol(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->writers_count--;
    if (node->readers_wait > 0) {
        node->can_readers_enter = 1;
        if ((err = pthread_cond_broadcast(&node->readers)) != 0) {
            syserr(err, "cond readers broadcast failed");
        }
    }
    else if (node->writers_wait > 0) {
        if ((err = pthread_cond_broadcast(&node->writers)) != 0) {
            syserr(err, "cond writers broadcast failed");
        }
    }
    else {
        node->can_readers_enter = 1;
    }

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}



Node *get_node(Node *node, const char *path, int type) {
//    printf("%s\n", path);
    if (!strcmp(path, "/")) {
        return node;
    }

    if (type == READER_BEGIN) {
        reader_beginning_protocol(node);
    }
    else if (type == READER_END) {
        reader_ending_protocol(node);
    }

    char *next_node_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    const char *new_path = split_path(path, next_node_name);

    Node *next_node = hmap_get(node->children, next_node_name);
    free(next_node_name);

    if (!next_node) {
        return NULL;
    }

    Node *result_node = get_node(next_node, new_path, type);
    return result_node;
}

int add_child(Node *parent, Node *child, const char *child_name) {
    if (hmap_get(parent->children, child_name)) {
        return EEXIST;
    }

    hmap_insert(parent->children, child_name, child);

    return 0;
}

//void remove_child_real(Node *node) {
//    assert(node->readers_count == 0 && node->readers_wait == 0 && node->writers_count == 0 && node->writers_wait == 0);
//
//    writer_beginning_protocol(node->parent);
//
//    hmap_remove(node->parent->children, node->name);
//
//    node_destroy(node); // TODO tutaj?
//
//    writer_ending_protocol(node->parent);
//}
//
//int remove_child(Node *parent, const char *child_name) {
//    Node *node = hmap_get(parent->children, child_name);
//
//    if (!node) {
//        return ENOENT;
//    }
//
//    writer_beginning_protocol(node);
//
//    if (hmap_size(node->children)) {
//        return ENOTEMPTY;
//    }
//
//    node->is_removed = true;
//    node->parent = parent;
//    char *name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
//    strcpy(name, child_name);
//    node->name = name;
//
//    writer_ending_protocol(node);
//
////    hmap_remove(parent->children, child_name);
////    node_destroy(node);
//
//    return 0;
//}



int remove_child(Node *parent, const char *child_name) {
    printf("remove_child\n");
    Node *node = hmap_get(parent->children, child_name);

    if (!node) {
        return ENOENT;
    }

    writer_beginning_protocol(node);

    if (hmap_size(node->children)) {
        writer_ending_protocol(node);
        return ENOTEMPTY;
    }

    hmap_remove(parent->children, child_name);
    node_destroy(node);

    return 0;
}

char *get_children_names(Node *node) {
//    printf("xd\n");
    return make_map_contents_string(node->children);
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
    printf("tree_list\n");

    if (!is_path_valid(path)) {
        return NULL;
    }

    Node *node = get_node(tree->root, path, READER_BEGIN);
    if (!node) {
        get_node(tree->root, path, READER_END);
        return NULL;
    }

    reader_beginning_protocol(node);

    char *result = get_children_names(node);

    reader_ending_protocol(node);

    get_node(tree->root, path, READER_END);

    return result;
}

int tree_create(Tree *tree, const char *path) {
    printf("tree_create\n");
    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        return EEXIST;
    }

    char *new_node_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_parent = make_path_to_parent(path, new_node_name);

    Node *parent = get_node(tree->root, path_to_parent, READER_BEGIN);
    if (!parent) {
        free(new_node_name);
        get_node(tree->root, path_to_parent, READER_END);
        free(path_to_parent);
        return ENOENT;
    }

    writer_beginning_protocol(parent);

    Node *new_node = node_new();
    int err = add_child(parent, new_node, new_node_name);
    free(new_node_name); // TODO ????
    if (err != 0) {
        node_destroy(new_node);
    }
    free(path_to_parent);

    writer_ending_protocol(parent);
    get_node(tree->root, path_to_parent, READER_END);
//    printf("tree_create: %d\n", err);
    return err;
}

int tree_remove(Tree *tree, const char *path) {
    printf("tree_remove\n");
    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        return EBUSY;
    }

    char *child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_parent = make_path_to_parent(path, child_name);

    printf("get_node start\n");
    Node *parent = get_node(tree->root, path_to_parent, READER_BEGIN);
    printf("get_node done\n");
    if (!parent) {
        free(child_name);
        get_node(tree->root, path_to_parent, READER_END);
        free(path_to_parent);
        return ENOENT;
    }

    printf("%d\n", parent->readers_count);
    printf("writer_beginning start\n");
    writer_beginning_protocol(parent);
    printf("writer_beginning end\n");

    int err = remove_child(parent, child_name);
    get_node(tree->root, path_to_parent, READER_END);
    free(child_name);
    free(path_to_parent);

    writer_ending_protocol(parent);
    get_node(tree->root, path_to_parent, READER_END);
//    printf("tree_remove: %d\n", err);
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
//    printf("tree_move\n");
    if (!is_path_valid(source) || !is_path_valid(target)) {
        return EINVAL;
    }
    if (!strcmp(source, "/")) {
        return EBUSY;
    }
    if (!strcmp(target, "/")) {
        return EEXIST;
    }

    if (is_substring(source, target)) {
//        free(source_child_name);
//        free(path_to_source_parent);
//        free(target_child_name);
//        free(path_to_target_parent);
        return -1;
    }


    char *source_child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_source_parent = make_path_to_parent(source, source_child_name);

    Node *source_parent_node = get_node(tree->root, path_to_source_parent, 0);
    if (!source_parent_node) {
        free(source_child_name);
        free(path_to_source_parent);
        return ENOENT;
    }



    Node *source_node = (Node *)hmap_get(source_parent_node->children, source_child_name);
    if (!source_node) {
//        print_map(tree->root->children);
//        printf("%s, %s\n", path_to_source_parent, source_child_name);
        free(source_child_name);
        free(path_to_source_parent);
        return ENOENT;
    }

    char *target_child_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *path_to_target_parent = make_path_to_parent(target, target_child_name);

    Node *target_parent_node = get_node(tree->root, path_to_target_parent, 0);

    if (!target_parent_node) {
        free(source_child_name);
        free(path_to_source_parent);
        free(target_child_name);
        free(path_to_target_parent);
        return ENOENT;
    }

    if (!strcmp(source, target)) {
        free(path_to_source_parent);
        free(path_to_target_parent);
        free(source_child_name);
        free(target_child_name);
        return 0;
    }

//    if (target_parent_node == source_node) {
//        return -1;
//    }


    int err = add_child(target_parent_node, source_node, target_child_name);

    if (!err) {
        hmap_remove(source_parent_node->children, source_child_name);
    }

    free(path_to_source_parent);
    free(path_to_target_parent);
    free(source_child_name);
    free(target_child_name);
//    printf("tree_move: %d\n", err);
    return err;
}
