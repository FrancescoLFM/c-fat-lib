#include <src/array.h>
#include <stdlib.h>

struct array *array_create(
        int capacity,
        void *(*copy)(void *),
        void (*destroy)(void *)
        )
{
    struct array *a;

    a = malloc(sizeof(*a));
    if (!a)
        return NULL;

    a->array = malloc(capacity * sizeof(*a->array));
    if (!a->array) {
        free(a);
        return NULL;
    }

    a->capacity = capacity;
    a->nmemb = 0;
    a->copy = copy;
    a->destroy = destroy;

    return a;
}

void array_destroy(struct array *a)
{
    while (array_remove(a) != ERROR);
    free(a->array);
    free(a);
}

int array_is_empty(struct array *a)
{
    return a->nmemb <= 0;
}

int array_is_full(struct array *a)
{
    return a->nmemb >= a->capacity;
}

err_t array_logresize(struct array *a)
{
    static const int FACTOR = 2;
    void **temp;

    temp = realloc(a->array, a->capacity * sizeof(*a->array) * FACTOR);
    if (!temp)
        return ERROR;

    a->array = temp;
    a->capacity *= FACTOR;

    return SUCCESS;
}

err_t array_push(struct array *a, void *element)
{
    void *copy;
    err_t error;

    if (array_is_full(a)) {
        error = array_logresize(a);
        if (error)
            return ERROR;
    }

    copy = a->copy(element);
    if (!copy)
        return ERROR;

    a->array[a->nmemb] = copy;
    a->nmemb++;

    return SUCCESS;
}

void *array_pop(struct array *a)
{
    if (array_is_empty(a))
        return NULL;

    a->nmemb--;
    return a->array[a->nmemb];
}

err_t array_remove(struct array *a)
{
    if (array_is_empty(a))
        return ERROR;

    a->nmemb--;
    a->destroy(a->array[a->nmemb]);

    return SUCCESS;
}
