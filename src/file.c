#include <src/fat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

void read_cluster(fat_file_t *file, uint32_t cluster, void *buffer)
{
    fat_t *fat = file->fs->fat;
    uint32_t data_start = fat->start + (fat->size * fat->count);

    if (cluster > file->fs->root_cluster)
        cluster -= file->fs->root_cluster;
        
    read_sectors(fat->drive->image, fat->drive->sector_per_cluster,
                 data_start + (cluster * fat->drive->sector_per_cluster), buffer);

    return;
}

uint32_t cluster_chain_read(fat_t *fat, uint32_t start, uint32_t pos)
{
    uint32_t chain_ring = start;

    do
    {
        chain_ring = fat_readl(fat, chain_ring);
    } while (chain_ring < EOC && pos--);

    return chain_ring;
}

void fat_file_cache_change(fat_file_t *file, size_t cache_size, uint32_t new_cluster)
{
    cache_refresh(file->cache, cache_size);
    read_cluster(file, new_cluster, file->cache);

    file->cluster_cache = new_cluster;

    return;
}

fat_file_t *fat_file_init(fat_fs_t *fs, fat_entry_t *entry)
{
    fat_file_t *file = malloc(sizeof(fat_file_t));

    file->info = fat_entry_copy(entry);
    file->fs = fs;
    file->cluster = entry->start_cluster;
    file->cluster_cache = entry->start_cluster;
    file->cache = malloc(fs->fat->drive->cluster_size);
    read_cluster(file, file->cluster, file->cache);
    file->eof = 0;

    return file;
}

void fat_file_close(fat_file_t *file)
{
    fat_entry_fini(file->info);
    free(file->cache);
    free(file);
}

uint8_t fat_file_readb(fat_file_t *file, uint32_t offset)
{
    uint32_t cluster;
    fat_t *fat = file->fs->fat;

    cluster = offset / fat->drive->cluster_size;
    offset %= fat->drive->cluster_size;
    if (cluster + file->cluster != file->cluster_cache)
    {
        if (cluster != 0) {
            cluster = cluster_chain_read(fat, file->cluster, cluster - 1);
            if (cluster >= EOC) {
                file->eof = 1;
                return 0;
            }
        }
        fat_file_cache_change(file, fat->drive->cluster_size, cluster);
    }

    return file->cache[offset];
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

fat_dir_t *fat_dir_open(fat_file_t *dir)
{
    fat_dir_t *file_entries = fat_dir_alloc(dir);
    uint32_t offset = 0;
    file_entries->fs = dir->fs;
    uint32_t attr;

    while (!dir->eof)
    {
        attr = fat_file_readb(dir, ATTR_OFFSET + offset);
        if (attr && attr != LFN)
            file_entries->entries[file_entries->entry_count++] = fat_entry_init(dir, offset);

        offset += ENTRY_SIZE;
    }

    return file_entries;
}

void fat_dir_close(fat_dir_t *dir)
{
    for (size_t i = 0; i < dir->entry_count; i++)
        fat_entry_fini(dir->entries[i]);
    free(dir->entries);
    free(dir);
}

int strcmp_insensitive(const char *s1, const char *s2)
{
    while (tolower(*s1) == tolower(*s2) && *s1 && *s2) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

fat_file_t *fat_file_open_recursive(fat_dir_t *dir)
{
    uint8_t is_dir;
    fat_file_t *ret = NULL;
    char *token = strtok_path(NULL, &is_dir);
    if (token == NULL)
        return NULL;

    for (size_t i=0; i < dir->entry_count; i++)
        if (!strcmp_insensitive(token, dir->entries[i]->name)) {
            fat_file_t *file = fat_file_init(dir->fs, dir->entries[i]);

            if (file->info->attributes & ARCHIVE) { /* Exit */
                if (is_dir) {
                    fat_file_close(file);
                    ret = NULL;
                }
                else
                    ret = file;
                goto exit;
            }
            
            else if (file->info->attributes & DIRECTORY) { /* Recur */
                fat_dir_t *new_dir = fat_dir_open(file);
                fat_file_t *new_file;

                new_file = fat_file_open_recursive(new_dir);

                fat_dir_close(new_dir);
                fat_file_close(file);
                ret = new_file;
                goto exit;
            }
            else {
                fat_file_close(file);
                ret = file;
                goto exit;
            }
        }

exit:
    free(token);
    return ret;
}

fat_entry_t *root_entry_init(fat_fs_t *fs)
{
    fat_entry_t *root_entry;

    root_entry = malloc(sizeof(fat_entry_t));

    root_entry->start_cluster = fs->root_cluster;
    root_entry->name = malloc(sizeof(char));
    *root_entry->name = '\0';
    root_entry->attributes = DIRECTORY;
    root_entry->creation_time = EMPTY_TIME;
    root_entry->creation_date = EMPTY_DATE;
    root_entry->last_access_date = EMPTY_DATE;
    root_entry->modification_time = EMPTY_TIME;
    root_entry->modification_date = EMPTY_DATE;
    root_entry->size = fat_dir_entry_get_size(fs, root_entry);
 
    return root_entry;
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

fat_dir_t *fat_dir_root_open(fat_fs_t *fs)
{
    fat_file_t *root_file;
    fat_dir_t *root_dir;
    fat_entry_t *root_entry;

    root_entry = root_entry_init(fs);
    root_file = fat_file_init(fs, root_entry);
    root_dir = fat_dir_open(root_file);

    fat_entry_fini(root_entry);
    fat_file_close(root_file);
    return root_dir;
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