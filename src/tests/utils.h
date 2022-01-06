#pragma once

#include "../Tree.h"

// Część funkcji przyjmuje maskę bitową operacji, z których ma generować.
// Taka maska bitowa jest or'em poniższych masek:
#define MASK_LIST (1 << 0)
#define MASK_CREATE (1 << 1)
#define MASK_REMOVE (1 << 2)
#define MASK_MOVE (1 << 3)

#define MASK_ALL ((1 << 4) - 1)

// Tworzy kolejną losową liczbę na podstawie kolejnej.
// Każda funkcja losująca ją wykonuje na koniec.
void advance_seed(int *curr_seed);

// Zwraca losową liczbę z przedziału [l, r].
int rd(int *curr_seed, int l, int r);

// Zwraca losową, małą ścieżkę
// (głębokość maksymalna 4, nazwa folderu to "a", "b" albo "c").
// Trzeba zwolnić otrzymany c-string.
char* get_random_small_path(int *curr_seed);

// Struktura przechowująca dane o losowo wygenerowanej operacji.
typedef struct Operation Operation;

// Zwraca losową operację (jedną wśród tych w masce)
// z losowymi ścieżkami z get_random_small_path
// (uruchomienie jej następuje dopiero po wywołaniu run_operation).
Operation* get_random_operation(int *curr_seed, int mask);

// Tworzy operację o danych argumentach.
Operation* construct_operation(int type, char *path1, char *path2);

void free_operation(Operation *operation);

int run_operation(Tree *tree, const Operation *operation);

// Wykonuje 100 losowych create'ów na drzewie, by go trochę wypełnić.
void run_some_creates(int *curr_seed, Tree *tree);

// Wypisuje na standardowe wyjście odpowiednie wywołanie funkcji.
void print_operation(const Operation *operation);

// Wypisuje na standardowe wyjście nazwę błędu.
void print_ret_code(int err);

