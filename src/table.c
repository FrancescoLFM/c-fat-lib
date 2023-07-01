#include <include/fat.h>
#include <stdlib.h>
#include <stdio.h>

#define FAT_CACHE_SIZE      10

fat_table_t *fat_table_init(fat_volume_t *volume)
{
    fat_table_t *table;

    table = malloc(sizeof(*table));
    if (table == NULL)
        puts("Malloc error: not enough space to allocate fat table");

    table->cache = cache_init(FAT_CACHE_SIZE, volume->sector_size);
    if (table->cache == NULL) {
        free(table);
        return NULL;
    }

    return table;
}

void fat_table_fini(fat_table_t *table, fat_volume_t *volume)
{
    cache_flush(table->cache, volume);
    cache_fini(table->cache);
    free(table);
}

uint32_t fat_table_access(fat_fs_t *fs, uint32_t cluster, uint32_t data, uint8_t mode)
{
    uint32_t offset;

    offset = cluster * sizeof(uint32_t);
    if (fs->table->size * fs->volume->sector_size <= offset)
        return READ_ERROR;

    if (mode == FAT_READ)
        return cache_readl(fs->table->cache, fs->volume, fs->table->address, offset);
    else if (mode == FAT_WRITE)
        cache_writel(fs->table->cache, fs->volume, fs->table->address, offset, data);

    return 0;
}

uint32_t fat_table_read(fat_fs_t *fs, uint32_t cluster)
{
    return fat_table_access(fs, cluster, 0, FAT_READ);
}

void fat_table_write(fat_fs_t *fs, uint32_t cluster, uint32_t content)
{
    fat_table_access(fs, cluster, content, FAT_WRITE);
}

uint32_t free_cluster_count_read(fat_fs_t *fs)
{
    uint32_t count = fs->info.root_cluster;

    for (uint32_t i=count; i < fs->volume->cluster_count; i++) {
        if (fat_table_read(fs, i) == 0) 
            count++;
    }

    return count;
}

uint32_t first_free_cluster_read(fat_fs_t *fs)
{
    uint32_t i;

    i = fs->info.root_cluster;
    while (fat_table_read(fs, i) != 0 && i++ < fs->volume->cluster_count);

    return i;
}

uint32_t fat_table_alloc_cluster(fat_fs_t *fs, uint32_t content)
{
    if (fs->info.free_cluster_count == 0)
        return CLUSTER_ALLOC_ERR;
    
    for (uint32_t i=fs->info.free_cluster; i < fs->volume->cluster_count; i++)
        if (fat_table_read(fs, i) == 0) {
            fat_table_write(fs, i, content);
            fs->info.free_cluster_count--;
            fs->info.free_cluster = i;
            return i;
        }

    return CLUSTER_ALLOC_ERR;
}