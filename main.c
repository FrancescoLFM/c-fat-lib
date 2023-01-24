#include <stdio.h>
#include <stdlib.h>
#include <src/fat.h>
#include <src/array.h>

#define ARR_SIZE(ARR)   (sizeof(ARR) / sizeof(*ARR))

void *int_copy(void *i)
{
    int *copy;

    copy = malloc(sizeof(int));
    if (!copy)
        return NULL;

   *copy = *(int *)i;
    return copy;
}

int main()
{
    struct array *arr;

    arr = array_create(1, int_copy, free);

    for (int i = 0; i < 10; i++)
        array_push(arr, &i);

    for (int i = 0; i < 10; i++) {
        int *num;

        num = arr->array[i];
        printf("%d\n", *num);
    }

    array_destroy(arr);

    return 0;
}
