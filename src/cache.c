#include "fat.h"
#include <stdlib.h>

int fat_cache_refresh(fat_cache_t *cache, fat_fs_t *fs, uint32_t new_address)
{
    int err = 0;

    if (cache->buffer != NULL) {
        if (cache->address != 0xFFFFFFFF)
            cache->write(cache, fs);
        free(cache->buffer);
    }

    cache->buffer = malloc(cache->size);
    if (cache->buffer == NULL) {
        fputs("fs_error: insufficient space to read the disk", stderr);
        err = 1;
    } else {
        cache->address = new_address;
        cache->read(cache, fs);
    }

    return err;
}

fat_cache_t *cache_init(size_t cache_size)
{
    fat_cache_t *cache;

    cache = malloc(sizeof(*cache));

    cache->address = 0xFFFFFFFF;
    cache->buffer = malloc(cache_size);
    cache->size = cache_size;

    return cache;
}

void fat_file_cache_read(fat_cache_t *cache, fat_fs_t *fs)
{
    read_cluster(fs, cache->address, cache->buffer);
}

void fat_file_cache_write(fat_cache_t *cache, fat_fs_t *fs)
{
    write_cluster(fs, cache->address, cache->buffer);
}

fat_cache_t *fat_file_cache_init(fat_file_t *file)
{
    fat_cache_t *cache;
    
    cache = cache_init(file->fs->fat->drive->cluster_size);
    cache->read = &fat_file_cache_read;
    cache->write = &fat_file_cache_write;

    return cache;
}

void fat_cache_read(fat_cache_t *cache, fat_fs_t *fs)
{
    read_sectors(fs->fat->drive->image, 1, cache->address, cache->buffer);
}

void fat_cache_write(fat_cache_t *cache, fat_fs_t *fs)
{
    write_sectors(fs->fat->drive->image, 1, cache->address, cache->buffer);
}

fat_cache_t *fat_cache_init(fat_t *fat)
{
    fat_cache_t *cache;

    cache = cache_init(fat->drive->sector_size);
    cache->read = &fat_cache_read;
    cache->write = &fat_cache_write;

    return cache;
}

void fat_cache_fini(fat_cache_t *cache, fat_fs_t *fs)
{
    cache->write(cache, fs);
    free(cache->buffer);
    free(cache);
}