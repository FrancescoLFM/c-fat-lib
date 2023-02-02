#include <stdio.h>
#include <stdlib.h>
#include <src/fat.h>
#include <src/array.h>

#define ARR_SIZE(ARR)   (sizeof(ARR) / sizeof(*ARR))
#define UNIX_YEAR       1980

void print_entry_time(fat_entry_time_t *time)
{
    printf("%d:%d:%d\n", time->hour, time->minutes, time->seconds);
    return;
}

void print_entry_date(fat_entry_date_t *date)
{
    printf("%d/%d/%d\n", date->day, date->month, UNIX_YEAR + date->year); 
    return;
}

void print_file(fat_file_t *file)
{
    printf("File name: %s\n", file->info->name);
    printf("File size: %d bytes\n", file->info->size);
    printf("File creation time: ");
    print_entry_time(&file->info->creation_time);
    printf("File creation date: ");
    print_entry_date(&file->info->creation_date);
}

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
        print_file(main_file);
        fat_file_close(main_file);
    }
    else
        puts("Invalid path...");

    fat_fs_fini(fs);
    fclose(image);
    return 0;
}
