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

int main(int argc, char **argv)
{
    if (argc != 2) {
        puts("Usage invalid");
        return 1;
    }

    FILE *image = fopen(IMAGE_NAME, "r");
    if (image == NULL)
        return -1;

    fat_fs_t *fs = fat_fs_init(image);

    fat_file_t *main_file = fat_file_open(fs, argv[1]);

    if (main_file != NULL) {
        printf("File opened: %s\n", main_file->info->name);
        fat_file_close(main_file);
    }
    else
        puts("Invalid path...");

    fat_fs_fini(fs);
    fclose(image);
    return 0;
}
