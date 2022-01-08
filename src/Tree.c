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

#define WRITER_ENTERS 0
#define READER_ENTERS 1
#define MOVER_ENTERS 2


typedef struct Node Node;

struct Node {
    HashMap *children;
    Node *parent;

    pthread_mutex_t mutex;
    pthread_cond_t writers;
    pthread_cond_t readers;
    pthread_cond_t movers;
    int readers_count, readers_wait, writers_count, writers_wait, movers_count, movers_wait, count_in_subtree, who_enters;
    bool is_removed;
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
    if ((err = pthread_cond_init(&node->movers, 0)) != 0) {
        syserr(err, "cond writers init failed");
    }

    node->readers_count = 0;
    node->writers_count = 0;
    node->readers_wait = 0;
    node->writers_wait = 0;
    node->who_enters = true;
    node->movers_count = 0;
    node->movers_wait = 0;
    node->count_in_subtree = 0;
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
    if ((err = pthread_cond_destroy(&node->movers)) != 0) {
        syserr(err, "mutex destroy failed");
    }

    free(node);
}



void reader_beginning_protocol(Node *node) {
    int err;
    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    if (node->who_enters != READER_ENTERS) {
        node->readers_wait++;
        while (node->who_enters != READER_ENTERS) {
            if ((err = pthread_cond_wait(&node->readers, &node->mutex)) != 0) {
                syserr(err, "cond readers wait failed");
            }
        }
        node->readers_wait--;
    }

    node->readers_count++;
    assert(node->readers_count == 0 || node->writers_count == 0);
    if (node->readers_wait == 0 && node->writers_wait > 0) {
        node->who_enters = WRITER_ENTERS;
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
    assert(node->readers_count >= 0);
    if (node->readers_count == 0 && node->writers_wait > 0) {
        node->who_enters = WRITER_ENTERS;
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

    node->who_enters = WRITER_ENTERS;

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
    node->who_enters = WRITER_ENTERS;
    assert(node->readers_count == 0 || node->writers_count == 0);

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
    assert(node->writers_count >= 0);
    if (node->readers_wait > 0) {
        node->who_enters = READER_ENTERS;
        if ((err = pthread_cond_broadcast(&node->readers)) != 0) {
            syserr(err, "cond readers broadcast failed");
        }
    }
    else if (node->writers_wait > 0) {
        node->who_enters = WRITER_ENTERS;
        if ((err = pthread_cond_broadcast(&node->writers)) != 0) {
            syserr(err, "cond writers broadcast failed");
        }
    }
    else {
        node->who_enters = READER_ENTERS;
    }

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}


void mover_beginning_protocol(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->who_enters = MOVER_ENTERS;

    if (node->count_in_subtree > 0) {
        node->movers_wait++;
        while (node->count_in_subtree > 0) {
            if ((err = pthread_cond_wait(&node->movers, &node->mutex)) != 0) {
                syserr(err, "cond movers wait failed");
            }
        }
        node->movers_wait--;
    }

    node->movers_count++;
    node->who_enters = MOVER_ENTERS;
    assert(node->readers_count == 0 && node->writers_count == 0 && node->count_in_subtree == 0);

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}



void mover_ending_protocol(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->movers_count--;
    assert(node->movers_count >= 0);
    if (node->writers_wait > 0) {
        node->who_enters = WRITER_ENTERS;
        if ((err = pthread_cond_broadcast(&node->writers)) != 0) {
            syserr(err, "cond writers broadcast failed");
        }
    }
    else if (node->readers_wait > 0) {
        node->who_enters = READER_ENTERS;
        if ((err = pthread_cond_broadcast(&node->readers)) != 0) {
            syserr(err, "cond readers broadcast failed");
        }
    }
    else {
        node->who_enters = READER_ENTERS;
    }
    assert(node->movers_wait == 0);

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}


void increase_counter(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->count_in_subtree++;

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}


void decrease_counter(Node *node) {
    int err;

    if ((err = pthread_mutex_lock(&node->mutex)) != 0) {
        syserr(err, "mutex lock failed");
    }

    node->count_in_subtree--;
    assert(node->count_in_subtree >= 0);

    if (node->count_in_subtree == 0 && node->movers_wait > 0) {
        if ((err = pthread_cond_broadcast(&node->movers)) != 0) {
            syserr(err, "cond movers broadcast failed");
        }
    }

    if ((err = pthread_mutex_unlock(&node->mutex)) != 0) {
        syserr(err, "mutex unlock failed");
    }
}


void decrease_counter_until(Node *node, Node *first_node) {
    while (node != first_node) {
        decrease_counter(node);
        node = node->parent;
    }
    decrease_counter(node);
}



Node *get_node_real(Node *node, Node *first_node, const char *path, int type, bool lock_first) {
//    printf("%s\n", path);
    increase_counter(node);

    if (!strcmp(path, "/")) {
        return node;
    }

    if (lock_first || first_node != node) {
        if (type == READER_BEGIN) {
            reader_beginning_protocol(node);
        }
        else if (type == READER_END) {
            assert(false);
            reader_ending_protocol(node);
        }
        else if (type == WRITER_BEGIN) {
            writer_beginning_protocol(node);
        }
        else if (type == WRITER_END) {
            assert(false);
            writer_ending_protocol(node);
        }
    }

    if (node != first_node && (lock_first || first_node != node->parent)) {
        if (type == READER_BEGIN) {
            reader_ending_protocol(node->parent);
        }
        else if (type == READER_END) {
            assert(false);
            reader_ending_protocol(node);
        }
        else if (type == WRITER_BEGIN) {
            writer_ending_protocol(node->parent);
        }
        else if (type == WRITER_END) {
            assert(false);
            writer_ending_protocol(node);
        }
    }

    char *next_node_name = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    const char *new_path = split_path(path, next_node_name);

    Node *next_node = hmap_get(node->children, next_node_name);
    free(next_node_name);

    if (!next_node) {
        if (lock_first || first_node != node) {
            if (type == READER_BEGIN) {
                reader_ending_protocol(node);
            }
            else if (type == READER_END) {
                assert(false);
                reader_ending_protocol(node);
            }
            else if (type == WRITER_BEGIN) {
                writer_ending_protocol(node);
            }
            else if (type == WRITER_END) {
                assert(false);
                writer_ending_protocol(node);
            }
        }

        decrease_counter_until(node, first_node);

        return NULL;
    }

    Node *result_node = get_node_real(next_node, first_node, new_path, type, lock_first);
    return result_node;
}

Node *get_node(Node *node, const char *path, int type, bool lock_first) {
    Node *result_node = get_node_real(node, node, path, type, lock_first);
    return result_node;
}

int add_child(Node *parent, Node *child, const char *child_name) {
    if (hmap_get(parent->children, child_name)) {
        return EEXIST;
    }

    hmap_insert(parent->children, child_name, child);
    child->parent = parent;

    return 0;
}


int remove_child(Node *parent, const char *child_name) {
//    printf("remove_child\n");
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
    tree->root->parent = NULL;
    return tree;
}

void tree_free(Tree *tree) {
    printf("%d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);
    node_destroy(tree->root);
    free(tree);
}

char *tree_list(Tree *tree, const char *path) {
    printf("tree_list begin: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);
//    printf("tree_list\n");

    if (!is_path_valid(path)) {
        return NULL;
    }

    Node *node = get_node(tree->root, path, READER_BEGIN, true);
    if (!node) {
//        get_node(tree->root, path, READER_END, true);
        return NULL;
    }

    reader_beginning_protocol(node);
    if (node->parent) {
        reader_ending_protocol(node->parent);
    }

    char *result = get_children_names(node);

//    get_node(tree->root, path, READER_END, true);

    reader_ending_protocol(node);

    decrease_counter_until(node, tree->root);

    printf("tree_list end: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

    return result;
}

int tree_create(Tree *tree, const char *path) {
//    printf("tree_create\n");
    printf("tree_create begin: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        return EEXIST;
    }

    char new_node_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *path_to_parent = make_path_to_parent(path, new_node_name);

    Node *parent = get_node(tree->root, path_to_parent, READER_BEGIN, true);
    if (!parent) {
//        get_node(tree->root, path_to_parent, READER_END, true);
        free(path_to_parent);
        return ENOENT;
    }

    writer_beginning_protocol(parent);
    if (parent->parent) {
        reader_ending_protocol(parent->parent);
    }

    Node *new_node = node_new();
    new_node->parent = parent;
    int err = add_child(parent, new_node, new_node_name);
    if (err != 0) {
        node_destroy(new_node);
    }

//    if (parent->parent != NULL) {
//        writer_ending_protocol(parent->parent);
//        reader_beginning_protocol(parent->parent);
//    }
//    get_node(tree->root, path_to_parent, READER_END, true);
    writer_ending_protocol(parent);

    decrease_counter_until(parent, tree->root);

    free(path_to_parent);
//    printf("tree_create: %d\n", err);
    printf("tree_create end: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

    return err;
}


int tree_remove(Tree *tree, const char *path) {
//    printf("tree_remove\n");
    printf("tree_remove begin: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (!strcmp(path, "/")) {
        return EBUSY;
    }

    char child_name[MAX_FOLDER_NAME_LENGTH + 1];;
    char *path_to_parent = make_path_to_parent(path, child_name);

//    printf("get_node start\n");
    Node *parent = get_node(tree->root, path_to_parent, READER_BEGIN, true);
//    printf("get_node done\n");
    if (!parent) {
//        get_node(tree->root, path_to_parent, READER_END, true);
        free(path_to_parent);
        return ENOENT;
    }
    //

    writer_beginning_protocol(parent);
    if (parent->parent) {
        reader_ending_protocol(parent->parent);
    }

    int err = remove_child(parent, child_name);
//    get_node(tree->root, path_to_parent, READER_END, true);
    free(path_to_parent);

    writer_ending_protocol(parent);

    decrease_counter_until(parent, tree->root);

//    printf("tree_remove: %d\n", err);
    printf("tree_remove end: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

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

void lock_subtree(Node *node, bool first) {
    if (!first) {
        writer_beginning_protocol(node);
    }
    const char *key = NULL;
    void *value = NULL;
    HashMapIterator it = hmap_iterator(node->children);
    while (hmap_next(node->children, &it, &key, &value)) {
        lock_subtree(value, false);
    }
}

void unlock_subtree(Node *node, bool first) {
    if (!first) {
        writer_ending_protocol(node);
    }
    const char *key = NULL;
    void *value = NULL;
    HashMapIterator it = hmap_iterator(node->children);
    while (hmap_next(node->children, &it, &key, &value)) {
        unlock_subtree(value, false);
    }
}

int tree_move(Tree *tree, const char *source, const char *target) {
//    printf("tree_move: %s, %s\n", source, target);
    printf("tree_move begin: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

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


    char *path_to_lca = make_path_to_lca(source, target);
    size_t diff = strlen(path_to_lca) - 1;

//    printf("get_lca: %s\n", path_to_lca);
    Node *lca_node = get_node(tree->root, path_to_lca, READER_BEGIN, true);
    if (!lca_node) {
        free(path_to_lca);
        printf("tree_move end 1: %d\n", tree->root->count_in_subtree);
        assert(tree->root->count_in_subtree == 0);
//        printf("xd1\n");
        return ENOENT;
    }

//    printf("writer lca\n");
    writer_beginning_protocol(lca_node);
    if (lca_node->parent) {
        reader_ending_protocol(lca_node->parent);
    }

//    printf("%s\n", (source + diff));
    char source_child_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *path_to_source_parent = make_path_to_parent(source + diff, source_child_name);

//    printf("get source parent \n");
//    printf("%d\n", tree->root->count_in_subtree);
    Node *source_parent_node = get_node(lca_node, path_to_source_parent, WRITER_BEGIN, false);
//    printf("%d\n", tree->root->count_in_subtree);
    if (!source_parent_node) {
        writer_ending_protocol(lca_node);
        decrease_counter_until(lca_node, tree->root);

        free(path_to_lca);
        free(path_to_source_parent);

        printf("tree_move end 2: %d\n", tree->root->count_in_subtree);
        assert(tree->root->count_in_subtree == 0);

        return ENOENT;
    }

    if (source_parent_node != lca_node) {
//        printf("writer source parent\n");
        writer_beginning_protocol(source_parent_node);
        if (source_parent_node->parent && source_parent_node->parent != lca_node) {
            writer_ending_protocol(source_parent_node->parent);
        }
    }

    Node *source_node = (Node *)hmap_get(source_parent_node->children, source_child_name);
    if (!source_node) {
//        printf("%d\n", tree->root->count_in_subtree);
        if (source_parent_node != lca_node) {
            writer_ending_protocol(source_parent_node);
        }
        decrease_counter_until(source_parent_node, lca_node);
//        printf("%d\n", tree->root->count_in_subtree);
        writer_ending_protocol(lca_node);
        decrease_counter_until(lca_node, tree->root);

        free(path_to_lca);
        free(path_to_source_parent);

        printf("tree_move end 3: %d\n", tree->root->count_in_subtree);
        assert(tree->root->count_in_subtree == 0);

        return ENOENT;
    }

    if (!strcmp(source, target)) {
        if (source_parent_node != lca_node) {
            writer_ending_protocol(source_parent_node);
        }
        decrease_counter_until(source_parent_node, lca_node);

        writer_ending_protocol(lca_node);
        decrease_counter_until(lca_node, tree->root);

        free(path_to_lca);
        free(path_to_source_parent);

        printf("tree_move end 4: %d\n", tree->root->count_in_subtree);
        assert(tree->root->count_in_subtree == 0);

        return 0;
    }


//    printf("writer source\n");
    writer_beginning_protocol(source_node);

    char target_child_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *path_to_target_parent = make_path_to_parent(target + diff, target_child_name);

//    printf("target_child_name: %s\n", target_child_name);

//    printf("get target parent: %s\n", path_to_target_parent);
    Node *target_parent_node;
    target_parent_node = get_node(lca_node, path_to_target_parent, WRITER_BEGIN, false);

    if (!target_parent_node) {
        writer_ending_protocol(source_node);
        if (source_parent_node != lca_node) {
            writer_ending_protocol(source_parent_node);
        }
        decrease_counter_until(source_parent_node, lca_node);
        writer_ending_protocol(lca_node);
        decrease_counter_until(lca_node, tree->root);


        free(path_to_lca);
        free(path_to_source_parent);
        free(path_to_target_parent);

        printf("tree_move end 5: %d\n", tree->root->count_in_subtree);
        assert(tree->root->count_in_subtree == 0);

        return ENOENT;
    }

//    printf("dupa\n");

    if (target_parent_node != lca_node) {
//        printf("writer target parent\n");
        writer_beginning_protocol(target_parent_node);
        if (target_parent_node && target_parent_node->parent != lca_node) {
            writer_ending_protocol(target_parent_node->parent);
        }
    }

//    print_map(target_parent_node->children);

    if (!strcmp(source, target)) {
        if (target_parent_node != lca_node) {
            writer_ending_protocol(target_parent_node);
        }
        decrease_counter_until(target_parent_node, lca_node);
        writer_ending_protocol(source_node);
        if (source_parent_node != lca_node) {
            writer_ending_protocol(source_parent_node);
        }
        decrease_counter_until(source_parent_node, lca_node);
        writer_ending_protocol(lca_node);
        decrease_counter_until(lca_node, tree->root);

        free(path_to_lca);
        free(path_to_source_parent);
        free(path_to_target_parent);

        printf("tree_move end 6: %d\n", tree->root->count_in_subtree);
        assert(tree->root->count_in_subtree == 0);

//        printf("beka\n");
        return 0;
    }

//    lock_subtree(source_node, true);

//    printf("cyce\n");

    int err = add_child(target_parent_node, source_node, target_child_name);

//    unlock_subtree(source_node, true);

//    printf("wadowice\n");

    if (!err) {
        hmap_remove(source_parent_node->children, source_child_name);
    }

//    printf("chuj\n");

    if (target_parent_node != lca_node) {
        writer_ending_protocol(target_parent_node);
    }
    decrease_counter_until(target_parent_node, lca_node);
    writer_ending_protocol(source_node);
    if (source_parent_node != lca_node) {
        writer_ending_protocol(source_parent_node);
    }
    decrease_counter_until(source_parent_node, lca_node);
    writer_ending_protocol(lca_node);
    decrease_counter_until(lca_node, tree->root);

    free(path_to_lca);
    free(path_to_source_parent);
    free(path_to_target_parent);

//    printf("koniec\n");

    printf("tree_move end: %d\n", tree->root->count_in_subtree);
    assert(tree->root->count_in_subtree == 0);

    return err;
}
