#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stdio.h>
#include <include/fat.h>

typedef struct cache_line cache_line_t;
typedef struct cache cache_t;
typedef struct fat_volume fat_volume_t;

struct cache_line 
{
    int valid;
    int tag;
    uint8_t *data;
};

struct cache
{
    size_t cache_size;
    size_t block_size;
    cache_line_t *lines;
};

cache_t *cache_init(size_t cache_size, size_t block_size);
cache_line_t *cache_lines_create(size_t line_count);
uint8_t cache_readb(cache_t *cache, fat_volume_t *volume, uint32_t sector, uint32_t offset);
uint32_t cache_readl(cache_t *cache, fat_volume_t *volume, uint32_t sector, uint32_t offset);
void cache_lines_destroy(cache_line_t *cache_lines, size_t line_count);
void cache_fini(cache_t *cache);

#endif