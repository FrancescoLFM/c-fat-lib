#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

typedef enum {
    SUCCESS = 0,
    ERROR
} err_t;

struct array {
    void **array;
    int nmemb;
    int capacity;

    void *(*copy)(void *);
    void (*destroy)(void *);
};

struct array *array_create(
        int capacity,
        void *(*copy)(void *),
        void (*destroy)(void *)
        );
void array_destroy(struct array *a);

int array_is_empty(struct array *a);
int array_is_full(struct array *a);

err_t array_logresize(struct array *a);
err_t array_push(struct array *a, void *element);
void *array_pop(struct array *a);
err_t array_remove(struct array *a);

#endif
