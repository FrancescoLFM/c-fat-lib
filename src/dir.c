#include "fat.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int strcmp_insensitive(const char *s1, const char *s2)
{
    while (tolower(*s1) == tolower(*s2) && *s1 && *s2) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

fat_dir_t *fat_dir_root_open(fat_fs_t *fs)
{
    fat_dir_t *root_dir;
    fat_entry_t *root_entry;

    root_entry = root_entry_init(fs);
    root_dir = fat_dir_open(fs, root_entry);

    fat_entry_fini(root_entry);
    return root_dir;
}


fat_dir_t *fat_dir_alloc(fat_file_t *dir_file)
{
    const uint32_t ENTRY_PER_CLUSTER = dir_file->fs->fat->drive->cluster_size / ENTRY_SIZE;
    fat_dir_t *dir = malloc(sizeof(fat_dir_t));
    if (dir == NULL)
        return NULL;

    dir->entry_count = 0;
    dir->max_entries = fat_dir_entry_get_size(dir_file->fs, dir_file->info) * ENTRY_PER_CLUSTER;
    dir->entries = malloc(sizeof(fat_entry_t *) * dir->max_entries);
    if (dir->entries == NULL) {
        free(dir);
        return NULL;
    }

    return dir;
}

fat_dir_t *fat_dir_open(fat_fs_t *fs, fat_entry_t *dir_entry)
{
    fat_file_t *dir_file;
    fat_dir_t *file_entries;
    uint32_t offset = 0;
    uint32_t attr;

    dir_file = fat_file_init(fs, dir_entry);
    file_entries = fat_dir_alloc(dir_file);
    file_entries->fs = fs;

    while (!dir_file->eof)
    {
        attr = fat_file_readb(dir_file, ATTR_OFFSET + offset);
        if (attr && attr != LFN)
            file_entries->entries[file_entries->entry_count++] = fat_entry_init(dir_file, offset);

        offset += ENTRY_SIZE;
    }

    fat_file_close(dir_file);
    return file_entries;
}

fat_entry_t *fat_dir_search(fat_dir_t *dir, char *filename)
{
    for (size_t i=0; i < dir->entry_count; i++)
        if (!strcmp_insensitive(dir->entries[i]->name, filename))
            return dir->entries[i];

    return NULL;
}

void fat_dir_close(fat_dir_t *dir)
{
    for (size_t i = 0; i < dir->entry_count; i++)
        fat_entry_fini(dir->entries[i]);
    free(dir->entries);
    free(dir);
}
