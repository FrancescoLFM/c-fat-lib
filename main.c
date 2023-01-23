#include <stdio.h>
#include <src/fat.h>

int main()
{
    FILE *image = fopen(IMAGE_NAME, "r+");

    fat_fs_t *fs = fat_fs_init(image);
    fat_file_t *main_file = fat_file_open(fs, "/MAIN.C");

    if (main_file != NULL)
        fat_file_close(main_file);
    fat_fs_fini(fs);
    fclose(image);
    return 0;
}