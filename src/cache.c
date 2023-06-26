#include <include/cache.h>
#include <stdlib.h>
#include <include/fat.h>

fat_volume_t *fat_volume_init(FILE *drive)
{
    fat_volume_t *volume;

    volume = malloc(sizeof(*volume));
    if (volume == NULL) {
        puts("Malloc error: not enough space to allocate volume struct");
        return NULL;
    }
    volume->drive = drive;
    volume->label = malloc(sizeof(char) * LABEL_LENGTH);
    if (volume->label == NULL) {
        puts("Malloc error: not enough space to allocate volume label");
        free(volume);
        return NULL;
    }
    volume->sector_count = UNDEFINED_SECCOUNT;
    volume->sector_size = SECTOR_SIZE;

    return volume;
}

uint8_t *read_sector(fat_volume_t *volume, uint32_t lba)
{
    uint8_t *buffer;

    if (lba > volume->sector_count) {
        puts("Disk error: tried reading off the bounds.");
        return NULL;
    }

    buffer = malloc(sizeof(uint8_t) * volume->sector_size);
    if (buffer == NULL) {
        puts("Malloc error: not enough space to allocate disk buffer");
        return NULL;
    }
    
    fseek(volume->drive, lba * volume->sector_size, SEEK_SET);
    fread(buffer, sizeof(uint8_t), volume->sector_size, volume->drive);

    return buffer;
}

void fat_volume_fini(fat_volume_t *volume)
{
    free(volume->label);
    free(volume);

    return;
}

cache_t *cache_init(size_t cache_size, size_t block_size)
{
    cache_t *cache;

    cache = malloc(sizeof(*cache));
    if (cache == NULL) {
        puts("Malloc error: not enough space to allocate cache");
        return NULL;
    }
    cache->cache_size = cache_size;
    cache->block_size = block_size;
    cache->lines = cache_lines_create(cache_size);
    if (cache->lines == NULL) {
        free(cache);
        return NULL;
    }

    return cache;
}

cache_line_t *cache_lines_create(size_t line_count)
{
    cache_line_t *cache_lines;

    cache_lines = calloc(sizeof(*cache_lines), line_count);
    if (cache_lines == NULL) {
        puts("Malloc error: not enough space to allocate cache lines vector");
        return NULL;
    }

    for (size_t i=0; i < line_count; i++) {
        cache_lines[i].valid = 0;
        cache_lines[i].tag = -1;
        cache_lines[i].data = NULL;
    }

    return cache_lines;
}

uint8_t cache_readb(cache_t *cache, fat_volume_t *volume, uint32_t sector, uint32_t offset)
{
    int tag;
    int index;

    index = sector % cache->cache_size;

    if (cache->lines[index].tag != sector) {
        if (cache->lines[index].valid != 0)
            free(cache->lines[index].data);
        
        cache->lines[index].data = read_sector(volume, sector);
        if (cache->lines[index].data != NULL) {
            cache->lines[index].tag = sector;
            cache->lines[index].valid = 1;
        } 
        else
            cache->lines[index].valid = 0;
    }

    if (cache->lines[index].valid == 0)
        return 0;

    return cache->lines[index].data[offset % cache->block_size];
}

uint32_t cache_readl(cache_t *cache, fat_volume_t *volume, uint32_t sector, uint32_t offset)
{
    return (cache_readb(cache, volume, sector, offset)     +
    (cache_readb(cache, volume, sector, offset + 1) << 8)  +
    (cache_readb(cache, volume, sector, offset + 2) << 16) +
    (cache_readb(cache, volume, sector, offset + 3) << 24));
}

void cache_lines_destroy(cache_line_t *cache_lines, size_t line_count)
{
    for (size_t i=0; i < line_count; i++)
        if (cache_lines[i].data != NULL)
            free(cache_lines[i].data);

    free(cache_lines);
}

void cache_fini(cache_t *cache)
{
    cache_lines_destroy(cache->lines, cache->cache_size);
    free(cache);
}