#include <stdio.h>
#include <src/fat.h>

int main()
{
    FILE *image = fopen(IMAGE_NAME, "r+");

    fat_fs_t *fs = fat_fs_init(image);
    fat_file_t *main_file = fat_file_open(fs, "/MAIN.TXT");
    fat_file_t *prova_file = fat_file_open(fs, "/PROVA/PROVA.TXT");

    if (main_file != NULL)
        fat_file_close(main_file);
    if (prova_file != NULL)
        fat_file_close(prova_file);
    fat_fs_fini(fs);
    fclose(image);
    return 0;
}