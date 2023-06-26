#include <include/fat.h>
#include <stdlib.h>
#include <string.h>

void fat_volume_getinfo(fat_volume_t *volume, uint8_t *info_buffer)
{
    volume->sector_size = BYTES_TO_WORD(info_buffer, BYTES_PER_SECTOR);

    if (info_buffer[SECTOR_COUNT] != 0)
        volume->sector_count = BYTES_TO_WORD(info_buffer, SECTOR_COUNT);
    else
        volume->sector_count = BYTES_TO_LONG(info_buffer, LARGE_SECTOR_COUNT);

    volume->cluster_size = info_buffer[SECTOR_PER_CLUSTER];
}

void fat_table_getinfo(fat_table_t *fat_table, uint8_t *info_buffer)
{
    fat_table->address = BYTES_TO_WORD(info_buffer, RESERVED_SECTORS);
    fat_table->size = BYTES_TO_LONG(info_buffer, FAT_TABLE_SIZE);
    fat_table->count = info_buffer[FAT_TABLE_COUNT];
}

void fat_volume_get_clusters(fat_volume_t *volume, fat_table_t *table)
{
    volume->cluster_count = (volume->sector_count - table->address - 
                            (table->size * table->count)) / volume->cluster_size;
}

uint8_t fat_fs_getinfo(fat_fs_t *fs)
{
    uint8_t *info_buffer;
    uint8_t *fsinfo_buffer;
    uint8_t err = 0;

    info_buffer = read_sector(fs->volume, BPB_SECTOR);
    if (info_buffer == NULL)
        return FS_ERROR;

    fsinfo_buffer = read_sector(fs->volume, BYTES_TO_WORD(info_buffer, FSINFO_SECTOR));
    if (fsinfo_buffer == NULL                                                || 
        BYTES_TO_LONG(fsinfo_buffer, LEAD_SIGNATURE1_OFF) != LEAD_SIGNATURE1 ||
        BYTES_TO_LONG(fsinfo_buffer, LEAD_SIGNATURE2_OFF) != LEAD_SIGNATURE2) {
        err = FS_ERROR;
        goto exit;
    }

    fat_volume_getinfo(fs->volume, info_buffer);
    fat_table_getinfo(fs->table, info_buffer);

    fat_volume_get_clusters(fs->volume, fs->table);

    fs->info.root_cluster = BYTES_TO_LONG(info_buffer, ROOT_CLUSTER);
    fs->info.free_cluster_count = BYTES_TO_LONG(fsinfo_buffer, FREE_CLUSTER_COUNT);
    if (fs->info.free_cluster_count == UNKNOWN_FREE_CLUSTER)
        fs->info.free_cluster_count = free_cluster_count_read(fs);
    fs->info.free_cluster = BYTES_TO_LONG(fsinfo_buffer, FREE_CLUSTER); 
    if (fs->info.free_cluster == UNKNOWN_FREE_CLUSTER)
        fs->info.free_cluster = first_free_cluster_read(fs);
        
exit:
    free(fsinfo_buffer);
    free(info_buffer);

    return err;
}

fat_fs_t *fat_fs_init(FILE *partition)
{
    fat_fs_t *fs;

    fs = malloc(sizeof(*fs));
    if (fs == NULL) {
        puts("Malloc error: not enough space to allocate filesystem");
        return NULL;
    }

    fs->volume = fat_volume_init(partition);
    if (fs->volume == NULL) {
        free(fs);
        return NULL;
    }
    fs->table = fat_table_init(fs->volume);
    if (fs->table == NULL) {
        fat_volume_fini(fs->volume);
        free(fs);
        return NULL;
    }

    if (fat_fs_getinfo(fs) == FS_ERROR) {
        puts("Fsinfo error: filesystem info structure is corrupted");
        fat_fs_fini(fs);
        return NULL;
    }

    return fs;
}

void fat_fs_fini(fat_fs_t *fs)
{
    fat_volume_fini(fs->volume);
    fat_table_fini(fs->table);
    free(fs);
}