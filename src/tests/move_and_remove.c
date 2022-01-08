/*
 * Prosty test tree_remove i tree_move.
 *
 * Test został stworzony, gdyż drugi podtest potrafił stwierdzić niepoprawność
 * rozwiązania, które przeszło pozostałe testy.
 *
 * Składa się z trzech podtestów:
 *
 * - w pierwszych dwóch podtestach jeden wątek tworzy i usuwa, a drugi
 *przenosi
 *
 * - w pierwszym podteście oba wątki powinny zgodzić się co do liczby udanych
 *przeniesień – sprawdza on atomowość operacji
 *
 * - w drugim przenoszenie nie dochodzi do skutku, ten podtest wykrywa
 * pewien specyficzny błąd
 *
 * - trzeci podtest stara się wykrywać i ułatwiać debugowanie deadlocków na
 * linii remove – move w głębi pliku można znaleźć ładny opis (ctrl+F `Wątki do
 * testu trzeciego`)
 *
 *
 * mail w razie pytań lub sugestii:
 * wd429167@students.mimuw.edu.pl
 */


#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h" // jedynie dla Tree.h

#define ITERATIONS 10000

#define mrm_msg(i) printf("- running move and remove test %d...\n", i)

void test1();
void test2();
void test3();

void move_and_remove() {
    test1();
    test2();
    test3();
}

// Wątki do testu pierwszego.

void *test1thread1(void *tree) {
    int *retval;
    assert((retval = (int *) malloc(sizeof *retval)));
    *retval = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        if (tree_move(tree, "/a/", "/b/") == 0)
            ++*retval;
    }
    // printf("moving done\n");
    return retval;
}

void *test1thread2(void *tree) {
    int *retval;
    assert((retval = (int *) malloc(sizeof *retval)));
    *retval = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        assert(tree_create(tree, "/a/") == 0);
        int res = tree_remove(tree, "/a/");
        if (res == 0) // we destroyed what we created
        {
            assert(tree_remove(tree, "/b/") == ENOENT);
        } else // the other thread moved it from under our nose
        {
            ++*retval;
            assert(res == ENOENT);
            assert(tree_remove(tree, "/b/") == 0);
        }
    }
    // printf("creating and removing done\n");
    return retval;
}

void test1() {
    mrm_msg(1);
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);
    Tree *t = tree_new();
    pthread_t th[2];
    assert(pthread_create(&th[0], &attr, test1thread1, t) == 0);
    assert(pthread_create(&th[1], &attr, test1thread2, t) == 0);

    void *rets[2];
    for (int i = 0; i < 2; ++i) {
        assert(pthread_join(th[i], &rets[i]) == 0);
    }
    assert(*(int *) rets[0] == *(int *) rets[1]); // liczba udanych move
    for (int i = 0; i < 2; ++i) {
        free(rets[i]);
    }
    tree_free(t);
}

// Wątki do testu drugiego.
// Jeden próbuje przenosić, drugi tworzy i usuwa.

void *test2thread1(void *tree) {
    for (int i = 0; i < ITERATIONS; ++i) {
        assert(tree_create(tree, "/a/") == 0);
        int res = tree_remove(tree, "/a/");
        if (res != 0)
            printf("res of remove: %d\n",
                   res); // informacja, jaki błąd nastąpił
        assert(res == 0);
    }
    // printf("creating and removing done\n");
    return NULL;
}

void *test2thread2(void *tree) {
    for (int i = 0; i < ITERATIONS; ++i) {
        assert(tree_move(tree, "/b/", "/a/b/") == ENOENT);
        assert(tree_move(tree, "/a/", "/b/a/") != 0);
    }
    // printf("moving done\n");
    return NULL;
}

void test2() {
    mrm_msg(2);
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);
    Tree *t = tree_new();
    pthread_t th[2];
    assert(pthread_create(&th[0], &attr, test2thread1, t) == 0);
    assert(pthread_create(&th[1], &attr, test2thread2, t) == 0);

    for (int i = 0; i < 2; ++i) {
        void *retval;
        assert(pthread_join(th[i], &retval) == 0);
        assert(retval == NULL);
    }

    tree_free(t);
}

/* Wątki do testu trzeciego.
 * Następujący podtest może ujawniać też deadlocki;
 * Raczej nie wykrywa deadlocków, których nie wykryje deadlock.c, ale wskazuje
 * Konkretnie na to, gdzie leży problem -- nie trzeba bawić się w detektywa.
 * Szkic:
 *     create: /a/, /a/a/, /b/ --- /a/a/ podtrzymuje /a/ przy życiu
 *     pierwszy wątek (w pętli):
 *         assert(remove(/a/) == ENOTEMPTY);
 *     drugi wątek (w pętli):
 *         assert(move(/b/, /a/b/) == 0);
 *         assert(move(/a/b/, /b/) == 0);
 */

void *test3thread1(void *tree) {
    for (int i = 0; i < ITERATIONS; ++i) {
        assert(tree_remove(tree, "/a/") == ENOTEMPTY);
    }
    return NULL;
}

void *test3thread2(void *tree) {
    for (int i = 0; i < ITERATIONS; ++i) {
        assert(tree_move(tree, "/b/", "/a/b/") == 0);
        assert(tree_move(tree, "/a/b/", "/b/") == 0);
    }
    return NULL;
}

void test3() {
    mrm_msg(3);
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);
    Tree *t = tree_new();
    tree_create(t, "/a/");
    tree_create(t, "/a/a/");
    tree_create(t, "/b/");
    pthread_t th[2];
    assert(pthread_create(&th[0], &attr, test3thread1, t) == 0);
    assert(pthread_create(&th[1], &attr, test3thread2, t) == 0);

    for (int i = 0; i < 2; ++i) {
        void *retval;
        assert(pthread_join(th[i], &retval) == 0);
        assert(retval == NULL);
    }

    tree_free(t);
}

