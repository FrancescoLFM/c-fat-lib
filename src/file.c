#include <include/fat.h>
#include <stdlib.h>

file_t *file_open(fat_fs_t *fs, entry_t *entry)
{
    file_t *file;

    file = malloc(sizeof(*file));
    if (file == NULL) {
        puts("Malloc error: not enough space to allocate file");
        return NULL;
    }

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

void file_close(fat_fs_t *fs, file_t *file) 
{
    cache_flush(file->cache, fs);
    cache_fini(file->cache);
    free(file);
}