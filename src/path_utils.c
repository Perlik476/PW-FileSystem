#include "path_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool is_path_valid(const char* path)
{
    size_t len = strlen(path);
    if (len == 0 || len > MAX_PATH_LENGTH)
        return false;
    if (path[0] != '/' || path[len - 1] != '/')
        return false;
    const char* name_start = path + 1; // Start of current path component, just after '/'.
    while (name_start < path + len) {
        char* name_end = strchr(name_start, '/'); // End of current path component, at '/'.
        if (!name_end || name_end == name_start || name_end > name_start + MAX_FOLDER_NAME_LENGTH)
            return false;
        for (const char* p = name_start; p != name_end; ++p)
            if (*p < 'a' || *p > 'z')
                return false;
        name_start = name_end + 1;
    }
    return true;
}

const char* split_path(const char* path, char* component)
{
    const char* subpath = strchr(path + 1, '/'); // Pointer to second '/' character.
    if (!subpath) // Path is "/".
        return NULL;
    if (component) {
        int len = subpath - (path + 1);
        assert(len >= 1 && len <= MAX_FOLDER_NAME_LENGTH);
        strncpy(component, path + 1, len);
        component[len] = '\0';
    }
    return subpath;
}

char* make_path_to_parent(const char* path, char* component)
{
    size_t len = strlen(path);
    if (len == 1) // Path is "/".
        return NULL;
    const char* p = path + len - 2; // Point before final '/' character.
    // Move p to last-but-one '/' character.
    while (*p != '/')
        p--;

    size_t subpath_len = p - path + 1; // Include '/' at p.
    char* result = malloc(subpath_len + 1); // Include terminating null character.
    strncpy(result, path, subpath_len);
    result[subpath_len] = '\0';

    if (component) {
        size_t component_len = len - subpath_len - 1; // Skip final '/' as well.
        assert(component_len >= 1 && component_len <= MAX_FOLDER_NAME_LENGTH);
        strncpy(component, p + 1, component_len);
        component[component_len] = '\0';
    }

    return result;
}

// A wrapper for using strcmp in qsort.
// The arguments here are actually pointers to (const char*).
static int compare_string_pointers(const void* p1, const void* p2)
{
    return strcmp(*(const char**)p1, *(const char**)p2);
}

const char** make_map_contents_array(HashMap* map)
{
    size_t n_keys = hmap_size(map);
    const char** result = calloc(n_keys + 1, sizeof(char*));
    HashMapIterator it = hmap_iterator(map);
    const char** key = result;
    void* value = NULL;
    while (hmap_next(map, &it, key, &value)) {
        key++;
    }
    *key = NULL; // Set last array element to NULL.
    qsort(result, n_keys, sizeof(char*), compare_string_pointers);
    return result;
}

char* make_map_contents_string(HashMap* map)
{
    const char** keys = make_map_contents_array(map);

    unsigned int result_size = 0; // Including ending null character.
    for (const char** key = keys; *key; ++key)
        result_size += strlen(*key) + 1;

    // Return empty string if children is empty.
    if (!result_size) {
        // Note we can't just return "", as it can't be free'd.
        char* result = malloc(1);
        *result = '\0';
        free(keys); // TODO czy dobrze
        return result;
    }

    char* result = malloc(result_size);
    char* position = result;
    for (const char** key = keys; *key; ++key) {
        size_t keylen = strlen(*key);
        assert(position + keylen <= result + result_size);
        strcpy(position, *key); // NOLINT: array size already checked.
        position += keylen;
        *position = ',';
        position++;
    }
    position--;
    *position = '\0';
    free(keys);
    return result;
}

bool is_substring(const char *a, const char *b) {
    if (strlen(a) >= strlen(b)) {
        return false;
    }

    size_t size_a = strlen(a);
    for (size_t i = 0; i < size_a; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

char *make_path_to_lca(const char *a, const char *b) {
    char *path = malloc(sizeof(char) * MAX_PATH_LENGTH);

    char *path_to_a_parent = make_path_to_parent(a, NULL);
    char *path_to_b_parent = make_path_to_parent(b, NULL);

    size_t size_a = strlen(path_to_a_parent);
    size_t size_b = strlen(path_to_b_parent);
    for (size_t i = 0; i < size_a && i < size_b; i++) {
        if (a[i] == b[i]) {
            path[i] = a[i];
        }
        else {
            path[i] = '\0';
            free(path_to_a_parent);
            free(path_to_b_parent);
            return path;
        }
    }
    path[size_a > size_b ? size_b : size_a] = '\0';
    free(path_to_a_parent);
    free(path_to_b_parent);
//    printf("xdddd\n");
//    printf("%s\n", path);

    return path;
}
