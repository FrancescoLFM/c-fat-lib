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

void fat_table_fini(fat_table_t *table)
{
    cache_fini(table->cache);
    free(table);
}

uint32_t fat_table_read(fat_fs_t *fs, uint32_t cluster)
{
    uint32_t offset;

    offset = cluster * sizeof(uint32_t);
    if (fs->table->size * fs->volume->sector_size <= offset)
        return READ_ERROR;

    return cache_readl(fs->table->cache, fs->volume, fs->table->address, offset);
}

uint32_t free_cluster_count_read(fat_fs_t *fs)
{
    uint32_t count = 0;

    for (uint32_t i=fs->info.root_cluster; i < fs->volume->cluster_count; i++) {
        if (fat_table_read(fs, i) == 0) 
            count++;
        else
            printf("Found allocated cluster: 0x%x\n", fat_table_read(fs, i));
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