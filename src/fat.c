#include <src/fat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void read_sectors(FILE *image, uint8_t n, uint32_t lba, void *buffer)
{
    fseek(image, lba * SECTOR_SIZE, SEEK_SET);
    fread(buffer, sizeof(uint8_t), n * SECTOR_SIZE, image);

    return;
}

void write_sectors(FILE *image, uint8_t n, uint32_t lba, void *buffer)
{
    fseek(image, lba * SECTOR_SIZE, SEEK_SET);
    fwrite(buffer, sizeof(uint8_t), n * SECTOR_SIZE, image);

    return;
}

uint32_t fat_readl(fat_fs_t *fs, uint32_t offset)
{
    uint32_t lba = fs->fat->start;
    uint32_t *cache_cast;

    lba += (offset * sizeof(uint32_t)) / fs->fat->drive->sector_size;
    offset %= (fs->fat->drive->sector_size / sizeof(uint32_t));

    if (lba != fs->fat->cache->address)
        fat_cache_refresh(fs->fat->cache, fs, lba);

    cache_cast = (uint32_t *) fs->fat->cache->buffer;

    return cache_cast[offset];
}

fat_drive_t *fat_drive_init(FILE *image, fat_bpb_t *params)
{
    fat_drive_t *drive = malloc(sizeof(fat_drive_t));

    drive->image = image;
    drive->sector_size = params->boot_record.sector_size;
    drive->sector_per_cluster = params->boot_record.cluster_size;
    drive->cluster_size = params->boot_record.cluster_size * params->boot_record.sector_size;
    drive->sector_count = params->boot_record.sector_count | params->boot_record.large_sector_count;
    drive->partition_lba = params->boot_record.hidden_sectors;

    return drive;
}

void fat_drive_fini(fat_drive_t *drive)
{
    free(drive);
}

fat_fs_t *fat_fs_init(FILE *image)
{
    fat_fs_t *fs = malloc(sizeof(fat_fs_t));
    fat_bpb_t boot_params;
    fat_fsinfo_t fsinfo;
    fat_drive_t *drive;

    fat_bpb_read(&boot_params, image);
    fs->root_cluster = boot_params.root_cluster;
    drive = fat_drive_init(image, &boot_params);

    fs->fat = fat_init(&boot_params, drive);

    uint8_t err = fsinfo_read(&fsinfo, &boot_params, image);
    if (err) {
        fs->free_cluster = 0xFFFFFFFF;
        fs->free_cluster_count = 0xFFFFFFFF;
    }
    else {
        fs->free_cluster = fsinfo.free_cluster;
        fs->free_cluster_count = fsinfo.free_cluster_count;
    }

    return fs;
}

void fat_fs_fini(fat_fs_t *fs)
{
    fat_fini(fs->fat, fs);
    free(fs);
}

void fat_bpb_read(fat_bpb_t *bpb, FILE *image)
{
    uint8_t *buffer = malloc(SECTOR_SIZE * sizeof(uint8_t));

    read_sectors(image, 1, 0, buffer);
    memcpy(bpb, buffer, sizeof(fat_bpb_t));

    free(buffer);
}

uint8_t fsinfo_read(fat_fsinfo_t *fsinfo, fat_bpb_t *bpb, FILE *image)
{
    uint8_t *buffer = malloc(SECTOR_SIZE * sizeof(uint8_t));
    uint8_t err = 0;

    read_sectors(image, 1, bpb->fsinfo_start, buffer);
    memcpy(fsinfo, buffer, sizeof(fat_fsinfo_t));

    if (fsinfo->lead_signature != FSINFO_LEAD ||
        fsinfo->mid_signature != FSINFO_MID ||
        fsinfo->trail_signature != FSINFO_TRAIL)
        err = 1;

    free(buffer);
    return err;
}

fat_t *fat_init(fat_bpb_t *fat_info, fat_drive_t *drive)
{
    fat_t *fat_table = malloc(sizeof(fat_t));

    fat_table->drive = drive;

    fat_table->start = fat_info->boot_record.fat_start;
    fat_table->size = fat_info->fat_size;
    fat_table->count = fat_info->boot_record.fat_count;

    fat_table->cache = fat_cache_init(fat_table);

    return fat_table;
}

void fat_fini(fat_t *fat, fat_fs_t *fs)
{
    fat_cache_fini(fat->cache, fs);
    fat_drive_fini(fat->drive);
    free(fat);
}

