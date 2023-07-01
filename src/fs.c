#include <include/fat.h>
#include <stdlib.h>
#include <string.h>

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

uint8_t *read_sector(fat_fs_t *fs, uint32_t lba)
{
    uint8_t *buffer;
    fat_volume_t *volume = fs->volume;

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
    fread(buffer, sizeof(*buffer), volume->sector_size, volume->drive);

    return buffer;
}

uint8_t *read_sectors(fat_fs_t *fs, uint32_t lba, uint32_t n)
{
    uint8_t *buffer;
    fat_volume_t *volume = fs->volume;

    if (lba + n - 1 > volume->sector_count) {
        puts("Disk error: tried reading off the bounds.");
        return NULL;
    }

    buffer = malloc(sizeof(uint8_t) * volume->sector_size * n);
    if (buffer == NULL) {
        puts("Malloc error: not enough space to allocate disk buffer");
        return NULL;
    }
    
    fseek(volume->drive, lba * volume->sector_size, SEEK_SET);
    fread(buffer, sizeof(*buffer), volume->sector_size * n, volume->drive);

    return buffer;
}

void write_sector(fat_fs_t *fs, uint32_t lba, uint8_t *buffer)
{
    fat_volume_t *volume = fs->volume;

    if (lba > volume->sector_count) {
        puts("Disk error: tried writing off the bounds.");
        return;
    }

    fseek(volume->drive, lba * volume->sector_size, SEEK_SET);
    fwrite(buffer, sizeof(*buffer), volume->sector_size, volume->drive);
}

void fat_volume_fini(fat_volume_t *volume)
{
    free(volume->label);
    free(volume);

    return;
}

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
                             (table->size * table->count)) /
                            volume->cluster_size;
}

uint8_t fat_fs_getinfo(fat_fs_t *fs)
{
    uint8_t *info_buffer;
    uint8_t err = 0;

    info_buffer = read_sector(fs, BPB_SECTOR);
    if (info_buffer == NULL)
        return FS_ERROR;

    fs->info.sector = BYTES_TO_WORD(info_buffer, FSINFO_SECTOR);
    fs->info.buffer = read_sector(fs, fs->info.sector);
    if (fs->info.buffer  == NULL ||
        BYTES_TO_LONG(fs->info.buffer , LEAD_SIGNATURE1_OFF) != LEAD_SIGNATURE1 ||
        BYTES_TO_LONG(fs->info.buffer , LEAD_SIGNATURE2_OFF) != LEAD_SIGNATURE2)
    {
        err = FS_ERROR;
        goto exit;
    }

    fat_volume_getinfo(fs->volume, info_buffer);
    fat_table_getinfo(fs->table, info_buffer);

    fat_volume_get_clusters(fs->volume, fs->table);

    fs->info.root_cluster = BYTES_TO_LONG(info_buffer, ROOT_CLUSTER);
    fs->info.free_cluster_count = BYTES_TO_LONG(fs->info.buffer , FREE_CLUSTER_COUNT);
    if (fs->info.free_cluster_count == UNKNOWN_FREE_CLUSTER)
        fs->info.free_cluster_count = free_cluster_count_read(fs);
    fs->info.free_cluster = BYTES_TO_LONG(fs->info.buffer , FREE_CLUSTER);
    if (fs->info.free_cluster == UNKNOWN_FREE_CLUSTER)
        fs->info.free_cluster = first_free_cluster_read(fs);
    fs->info.data_region = (fs->table->size * fs->table->count) + fs->table->address;

exit:
    free(info_buffer);

    return err;
}

void fat_fsinfo_flush(fat_fs_t *fs)
{
    memcpy(fs->info.buffer + FREE_CLUSTER_COUNT, 
          &fs->info.free_cluster_count, sizeof(fs->info.free_cluster_count));
    memcpy(fs->info.buffer + FREE_CLUSTER, 
          &fs->info.free_cluster, sizeof(fs->info.free_cluster));

    write_sector(fs, fs->info.sector, fs->info.buffer);
}

fat_fs_t *fat_fs_init(FILE *partition)
{
    fat_fs_t *fs;

    fs = malloc(sizeof(*fs));
    if (fs == NULL)
    {
        puts("Malloc error: not enough space to allocate filesystem");
        return NULL;
    }

    fs->volume = fat_volume_init(partition);
    if (fs->volume == NULL)
    {
        free(fs);
        return NULL;
    }
    fs->table = fat_table_init(fs->volume);
    if (fs->table == NULL)
    {
        fat_volume_fini(fs->volume);
        free(fs);
        return NULL;
    }

    if (fat_fs_getinfo(fs) == FS_ERROR)
    {
        puts("Fsinfo error: filesystem info structure is corrupted");
        fat_fs_fini(fs);
        return NULL;
    }

    return fs;
}

void fat_fs_fini(fat_fs_t *fs)
{
    if (fs->info.buffer != NULL) {
        fat_fsinfo_flush(fs);
        free(fs->info.buffer);
    }
    fat_table_fini(fs->table, fs);
    fat_volume_fini(fs->volume);
    free(fs);
}

uint8_t *read_cluster(fat_fs_t *fs, uint32_t cluster)
{
    uint32_t offset;
    uint8_t *buffer;

    if (cluster > fs->volume->cluster_count) {
        puts("Disk error: reading cluster off the bounds");
        return NULL;
    }

    offset = cluster * fs->volume->cluster_size;

    buffer = read_sectors(fs, fs->info.data_region + offset, fs->volume->cluster_size);
    if (buffer == NULL)
        return NULL;
    
    return buffer;
}

void write_cluster(fat_fs_t *fs, uint32_t cluster, uint8_t *buffer)
{

}