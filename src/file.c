#include <src/fat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

void access_cluster(fat_fs_t *fs, uint32_t cluster, void *buffer, uint8_t direction)
{
    fat_t *fat = fs->fat;
    uint32_t data_start = fat->start + (fat->size * fat->count);
    uint32_t lba;

    if (cluster >= fs->root_cluster)
        cluster -= fs->root_cluster;

    lba = data_start + (cluster * fat->drive->sector_per_cluster);

    if (direction == FILE_READ)
        read_sectors(fat->drive->image, fat->drive->sector_per_cluster,
                     lba, buffer);
    else if (direction == FILE_WRITE)
        write_sectors(fat->drive->image, fat->drive->sector_per_cluster,
                      lba, buffer);

    return;
}

void read_cluster(fat_fs_t *fs, uint32_t cluster, void *buffer)
{
    access_cluster(fs, cluster, buffer, FILE_READ);
}

void write_cluster(fat_fs_t *fs, uint32_t cluster, void *buffer)
{
    access_cluster(fs, cluster, buffer, FILE_WRITE);
}

uint32_t cluster_chain_read(fat_fs_t *fs, uint32_t start, uint32_t pos)
{
    uint32_t chain_ring = start;

    do {
        chain_ring = fat_readl(fs, chain_ring);
    } while (chain_ring < EOC && pos--);

    return chain_ring;
}

fat_file_t *fat_file_init(fat_fs_t *fs, fat_entry_t *entry)
{
    fat_file_t *file = malloc(sizeof(fat_file_t));

    file->info = fat_entry_copy(entry);
    file->fs = fs;
    file->cluster = entry->start_cluster;
    file->cache = fat_file_cache_init(file);
    file->eof = 0;

    return file;
}

void fat_file_close(fat_file_t *file)
{
    fat_entry_fini(file->info);
    fat_cache_fini(file->cache, file->fs);
    free(file);
}

uint8_t fat_file_access(fat_file_t *file, uint32_t offset, uint8_t val, uint8_t direction)
{
    uint32_t cluster;
    fat_t *fat = file->fs->fat;
    uint8_t *cache_cast;

    cluster = offset / fat->drive->cluster_size;
    offset %= fat->drive->cluster_size;
    if (cluster + file->cluster != file->cache->address)
    {
        if (cluster != 0) {
            cluster = cluster_chain_read(file->fs, file->cluster, cluster - 1);
            if (cluster >= EOC) {
                file->eof = 1;
                return 0;
            }
            fat_cache_refresh(file->cache, file->fs, cluster);
        }
        else
            fat_cache_refresh(file->cache, file->fs, file->cluster);
    }

    cache_cast = (uint8_t *) file->cache->buffer;

    if (direction == FILE_READ)
        return cache_cast[offset];
    else if (direction == FILE_WRITE)
        cache_cast[offset] = val;

    return 0;
}


uint8_t fat_file_readb(fat_file_t *file, uint32_t offset)
{
    uint8_t ret;

    ret = fat_file_access(file, offset, 0, FILE_READ);

    return ret;
}

uint16_t fat_file_readw(fat_file_t *file, uint32_t offset)
{
    return (fat_file_readb(file, offset) | (fat_file_readb(file, offset+1) << 8));
}

uint16_t fat_file_readl(fat_file_t *file, uint32_t offset)
{
    return (fat_file_readw(file, offset) | (fat_file_readb(file, offset+2) << 16));
}

void fat_file_buffered_readb(fat_file_t *file, uint32_t offset, uint8_t *buffer, size_t buffer_size)
{
    for (size_t i=0; i < buffer_size; i++)
        buffer[i] = fat_file_readb(file, offset + i);

    return;
}

void fat_file_writeb(fat_file_t *file, uint32_t offset, uint8_t val)
{
    fat_file_access(file, offset, val, FILE_WRITE);

    return;
}

fat_file_t *fat_file_open_recursive(fat_dir_t *dir)
{
    fat_file_t *ret = NULL;
    fat_entry_t *file_entry;
    uint8_t is_dir;
    char *token;

    token = strtok_path(NULL, &is_dir);
    if (token == NULL)
        return NULL;

    file_entry = fat_dir_search(dir, token);
    if (file_entry == NULL)
        goto exit;

    if (file_entry->attributes == ARCHIVE && !is_dir)
        ret = fat_file_init(dir->fs, file_entry);
        
    else if (file_entry->attributes == DIRECTORY && is_dir) {
        fat_dir_t *new_dir;

        new_dir = fat_dir_open(dir->fs, file_entry);

        ret = fat_file_open_recursive(new_dir);
        fat_dir_close(new_dir);
    }

exit:
    free(token);
    return ret;
}

char *strtok_path(char *path, uint8_t *is_dir)
{
    static char *path_saved = NULL;
    static int path_pos = 0;

    char *token; 
    size_t token_len = 0;
    int last_pos;

    if (is_dir) *is_dir = 0;

    if (path != NULL)
        path_saved = path;

    if (path_saved == NULL || path_saved[path_pos] == '\0')
        return NULL;

    last_pos = path_pos;

    while (path_saved[path_pos] != '\0') {
        if (path_saved[path_pos++] == '/') {
            if (is_dir) *is_dir = 1;
            break;
        }
        token_len++;
    }
    
    token = strndup(&path_saved[last_pos], token_len);

    return token;
}

fat_file_t *fat_file_open(fat_fs_t *fs, char *path)
{
    fat_dir_t *operating_dir = NULL;
    fat_file_t *file = NULL;
    char *token;

    token = strtok_path(path, NULL);
    
    switch (*path)
    {
    case '/':
        operating_dir = fat_dir_root_open(fs);
        break;

    default:
        /* Implement relative path */
        goto exit;
    }

    file = fat_file_open_recursive(operating_dir);

    fat_dir_close(operating_dir);

exit:
    free(token);
    return file;
}