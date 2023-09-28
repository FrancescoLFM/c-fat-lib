#include <include/fat.h>
#include <stdlib.h>
#include <string.h>

file_t *file_open(fat_fs_t *fs, entry_t *entry)
{
    file_t *file;

    file = malloc(sizeof(*file));
    if (file == NULL) {
        puts("Malloc error: not enough space to allocate file");
        return NULL;
    }

    file->path = NULL;
    file->entry = entry;
    file->cache = cache_init((entry->size / fs->volume->cluster_sizeb) + 1, fs->volume->cluster_sizeb, 
                              read_cluster, write_cluster);
    if (file->cache == NULL) {
        puts("File error: not enough space to init the file cache");
        free(file);
        return NULL;
    }
    file->cluster = WORDS_TO_LONG(entry->high_cluster, entry->low_cluster);

    return file;
}

uint8_t file_readb(file_t *file, fat_fs_t *fs, uint32_t offset)
{
    uint32_t cluster;

    if (offset > file->entry->size)
        return FAT_EOF;

    cluster = cluster_chain_read(fs, file->cluster, offset / fs->volume->cluster_sizeb);
    return cache_readb(file->cache, fs, cluster, offset);
}

uint8_t *file_read(file_t *file, fat_fs_t *fs, uint32_t offset, size_t size)
{
    uint8_t *buffer;

    buffer = malloc(size * sizeof(uint8_t));
    if (buffer == NULL) {
        puts("Memory error: not enough space to save file buffer");
        return NULL;
    }

    for (size_t i=0; i < size; i++)
        buffer[i] = file_readb(file, fs, offset + i);

    return buffer;
}

void file_writeb(file_t *file, fat_fs_t *fs, uint32_t offset, uint8_t data)
{
    uint32_t cluster;

    if (offset > file->entry->size)
        return;

    cluster = cluster_chain_read(fs, file->cluster, offset / fs->volume->cluster_sizeb);
    return cache_writeb(file->cache, fs, cluster, offset, data);
}

void file_write(file_t *file, fat_fs_t *fs, uint32_t offset, uint8_t *data, size_t size)
{
    for (size_t i=0; i < size; i++)
        file_writeb(file, fs, offset + i, data[i]);
}

file_t *file_open_path(fat_fs_t *fs, char *path)
{
    entry_t *entry;
    dir_t *starting_dir;
    char *save_path = path;
    file_t *ret;

    /* root check */
    if (*path == '/') {
        starting_dir = dir_init(fs, fs->root_entry);
        ++path;
    }
    else
        return NULL;

    dir_scan(fs, starting_dir);
    entry = dir_search_path(fs, starting_dir, path);
    dir_close(fs, starting_dir);

    if (entry == NULL || entry->attr != FILE_ATTR)
        return NULL;

    ret = file_open(fs, entry);
    ret->path = strdup(save_path);
    free(entry);
    return ret;
}

void entry_name_copy(entry_t *entry, char *filename)
{
    int j;

    for (int i = 0; i < SHORT_NAME_LEN; i++)
        entry->short_name[i] = ' ';

    for (j = 0; j < FILENAME_LEN && filename[j] && filename[j] != '.'; j++)
        entry->short_name[j] = filename[j];

    for (int k = 0; k < FILE_EXT_LEN && filename[j]; k++, j++)
        entry->short_name[SHORT_NAME_LEN + k] = filename[j];

    return;
}

entry_t *file_entry_create(char *filename, uint32_t cluster)
{
    entry_t *entry;

    entry = malloc(sizeof(*entry));
    if (entry == NULL)
        return NULL;
        
    memset(entry, 0, sizeof(*entry));
    entry_name_copy(entry, filename);

    entry->low_cluster = cluster & 0xFFFF;
    entry->high_cluster = cluster >> 16;
    entry->attr = FILE_ATTR;

    return entry;
}

void file_create(fat_fs_t *fs, char *path, char *filename)
{
    uint32_t cluster;
    entry_t *dir_entry;
    dir_t *starting_dir;
    dir_t *dir;
    entry_t *file_entry;

    cluster = first_free_cluster_read(fs);
    
    /* root check */
    if (*path == '/') {
        starting_dir = dir_init(fs, fs->root_entry);
        ++path;
    }
    else
        return;

    dir_scan(fs, starting_dir);
    dir_entry = dir_search_path(fs, starting_dir, path);
    if (dir_entry == NULL)
        goto exit;

    dir = dir_init(fs, dir_entry);
    if (dir == NULL) {
        free(dir_entry);
        goto exit;
    }
    dir_scan(fs, dir);
    if ((file_entry = dir_search(dir, filename)) != NULL) {
        free(file_entry);
        free(dir_entry);
        dir_close(fs, dir);
        goto exit;
    }
    file_entry = file_entry_create(filename, cluster);
    if (file_entry == NULL) {
        free(dir_entry);
        dir_close(fs, dir);
        goto exit;
    }
    
    fat_table_alloc_cluster(fs, EOC1);
    dir_entry_create(fs, dir, file_entry);

    dir_close(fs, dir);
    free(dir_entry);
    free(file_entry);
exit:
    dir_close(fs, starting_dir);
    return;
}

void file_close(fat_fs_t *fs, file_t *file) 
{
    if (file->path != NULL)
        free(file->path);
    cache_flush(file->cache, fs);
    cache_fini(file->cache);
    free(file);
}