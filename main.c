#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_NAME "filesystem.img"
#define SECTOR_SIZE 512
#define OEM_LEN 8

#define FSINFO_LEAD 0x41615252
#define FSINFO_MID 0x61417272
#define FSINFO_TRAIL 0xAA550000

#define EOC 0xffffff8

#define NAME_OFFSET 0
#define NAME_LEN    11
#define ATTR_OFFSET 11

#define ENTRY_SIZE 32


typedef struct
{
    uint8_t jmp[3];
    char oem[OEM_LEN];
    uint16_t sector_size; // In bytes
    uint8_t cluster_size; // In sectors
    uint16_t fat_start;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t sector_count;
    uint8_t media_desc_type;
    uint16_t fat_size;   // FAT 12/16 only
    uint16_t track_size; // In sectors
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t large_sector_count;
} __attribute__((packed)) bpb_t;

typedef struct
{
    bpb_t boot_record;
    uint32_t fat_size;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fsinfo_start;
    uint16_t boot_backup_lba;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved_flags;
    uint8_t signature;
    uint32_t serial;
    char volume_label[11];
    char identifier[8];
} __attribute__((packed)) fat_bpb_t;

typedef struct
{
    uint32_t lead_signature;
    uint8_t reserved1[480];
    uint32_t mid_signature;
    uint32_t free_cluster_count;
    uint32_t free_cluster;
    uint8_t reserved2[12];
    uint32_t trail_signature;
} __attribute__((packed)) fat_fsinfo_t;

typedef struct
{
    FILE *image;
    uint16_t sector_size;  // In bytes
    uint32_t cluster_size; // In bytes
    uint32_t sector_count;
    uint32_t partition_lba;
} fat_drive_t;

typedef struct
{
    fat_drive_t *drive;
    uint16_t start;
    uint8_t count;
    uint32_t size;
    uint32_t cache_lba;
    uint32_t *cache;
} fat_t;

typedef struct
{
    fat_t *fat;
    uint32_t root_cluster;
    uint32_t free_cluster_count;
    uint32_t free_cluster;
} fat_fs_t;

typedef struct
{
    fat_fs_t *fs;
    uint32_t cluster;
    uint32_t cluster_cache;
    uint8_t *cache;
    uint8_t eof;
} fat_file_t;

typedef struct
{
    char name[11];
    uint8_t attributes;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint32_t start_cluster;
    uint16_t modification_time;
    uint16_t modification_date;
    uint32_t size; // In bytes
} fat_file_entry_t;

typedef struct
{
    fat_file_entry_t **entries;
    size_t entry_count;
} fat_dir_t;

void read_sectors(FILE *image, uint8_t n, uint32_t lba, void *buffer)
{
    fseek(image, lba * SECTOR_SIZE, SEEK_SET);
    fread(buffer, sizeof(uint8_t), n * SECTOR_SIZE, image);

    return;
}

void read_cluster(fat_file_t *file, uint32_t cluster, void *buffer)
{
    fat_t *fat = file->fs->fat;
    uint32_t data_start = fat->start + (fat->size * fat->count);

    if (cluster < file->fs->root_cluster)
        return;
    cluster -= file->fs->root_cluster;
    read_sectors(fat->drive->image, fat->drive->cluster_size / fat->drive->sector_size,
                 data_start + (cluster * fat->drive->cluster_size), buffer);

    return;
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

    read_sectors(image, 1, bpb->fsinfo_start, buffer);
    memcpy(fsinfo, buffer, sizeof(fat_fsinfo_t));

    if (fsinfo->lead_signature != FSINFO_LEAD ||
        fsinfo->mid_signature != FSINFO_MID ||
        fsinfo->trail_signature != FSINFO_TRAIL)
        return 1;

    free(buffer);
    return 0;
}

void cache_refresh(void *cache, size_t cache_size)
{
    if (cache != NULL)
        free(cache);

    cache = malloc(cache_size);
    if (cache == NULL)
    {
        fputs("fs_error: insufficient space to read the disk", stderr);
        return;
    }

    return;
}

void fat_cache_change(fat_t *fat, uint32_t new_lba)
{
    cache_refresh(fat->cache, fat->drive->sector_size);
    read_sectors(fat->drive->image, 1, new_lba, fat->cache);

    fat->cache_lba = new_lba;
}

uint32_t fat_readl(fat_t *fat, uint32_t offset)
{
    uint32_t lba = fat->start;

    lba += (offset * sizeof(uint32_t)) / fat->drive->sector_size;
    offset %= (fat->drive->sector_size / sizeof(uint32_t));

    if (fat->cache_lba != lba)
        fat_cache_change(fat, lba);

    return fat->cache[offset];
}

fat_drive_t *fat_drive_init(FILE *image, fat_bpb_t *params)
{
    fat_drive_t *drive = malloc(sizeof(fat_drive_t));

    drive->image = image;
    drive->sector_size = params->boot_record.sector_size;
    drive->cluster_size = params->boot_record.cluster_size * params->boot_record.sector_size;
    drive->sector_count = params->boot_record.sector_count | params->boot_record.large_sector_count;
    drive->partition_lba = params->boot_record.hidden_sectors;

    return drive;
}

void fat_drive_fini(fat_drive_t *drive)
{
    free(drive);
}

fat_t *fat_init(fat_bpb_t *fat_info, fat_drive_t *drive)
{
    fat_t *fat_table = malloc(sizeof(fat_t));

    fat_table->drive = drive;

    fat_table->start = fat_info->boot_record.fat_start;
    fat_table->size = fat_info->fat_size;
    fat_table->count = fat_info->boot_record.fat_count;

    fat_table->cache = malloc(sizeof(uint8_t) * fat_table->drive->sector_size);
    read_sectors(drive->image, 1, fat_table->start, fat_table->cache);
    fat_table->cache_lba = fat_table->start;

    return fat_table;
}

void fat_fini(fat_t *fat)
{
    fat_drive_fini(fat->drive);
    free(fat->cache);
    free(fat);
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
    fat_fini(fs->fat);
    free(fs);
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

fat_file_t *fat_file_init(fat_fs_t *fs, uint32_t cluster)
{
    fat_file_t *file = malloc(sizeof(fat_file_t));

    file->fs = fs;
    file->cluster = cluster;
    file->cluster_cache = cluster;
    file->cache = malloc(fs->fat->drive->cluster_size);
    read_cluster(file, cluster, file->cache);
    file->eof = 0;

    return file;
}

void fat_file_fini(fat_file_t *file)
{
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

uint8_t fat_entry_read_attr(fat_file_t *dir, uint32_t offset)
{
    uint8_t attr_byte;
    
    attr_byte = fat_file_readb(dir, ATTR_OFFSET + offset);

    return attr_byte;
}

fat_file_entry_t *fat_entry_init(fat_file_t *dir, uint32_t dir_offset)
{
    fat_file_entry_t *file_entry = malloc(sizeof(fat_file_entry_t));

    fat_file_buffered_readb(dir, NAME_OFFSET + dir_offset, (uint8_t *)file_entry->name, NAME_LEN );
    file_entry->attributes = fat_file_readb(dir, ATTR_OFFSET + dir_offset);
    file_entry->creation_time = fat_file_readw(dir, 14 + dir_offset);
    file_entry->creation_date = fat_file_readw(dir, 16 + dir_offset);
    file_entry->last_access_date = fat_file_readw(dir, 18 + dir_offset);
    file_entry->start_cluster = (
                                fat_file_readw(dir, 26 + dir_offset)       |
                                fat_file_readw(dir, 20 + dir_offset) << 16
                                );
    file_entry->modification_time = fat_file_readw(dir, 22 + dir_offset);
    file_entry->modification_date = fat_file_readw(dir, 24 + dir_offset);
    file_entry->size = fat_file_readl(dir, 28 + dir_offset);

    return file_entry;
}

fat_file_entry_t **fat_dir_read(fat_file_t *dir)
{
    fat_file_entry_t **file_entries = malloc(sizeof(fat_file_entry_t *) * 8);

    uint32_t file_n = 0;
    uint32_t offset = 0;

    while (!dir->eof)
    {
        uint32_t attr;
        if ((attr = fat_entry_read_attr(dir, offset)) == 0x0F) { // File is a LFN, skip
            offset += ENTRY_SIZE;
        }
        else if (!attr) { // file entry doesn't exist, skip
            offset += ENTRY_SIZE;
            continue;
        }
        file_entries[file_n] = fat_entry_init(dir, offset);
        
        offset += ENTRY_SIZE;
        file_n++;
    }

    return file_entries;
}

int main()
{
    FILE *image = fopen(IMAGE_NAME, "r+");

    fat_fs_t *fs = fat_fs_init(image);
    fat_file_t *root = fat_file_init(fs, fs->root_cluster);
    //fat_file_entry_t **root_entries = fat_dir_read(root);
    // (void) root_entries;
    (void) root;

    fat_file_fini(root);
    fat_fs_fini(fs);
    fclose(image);
    return 0;
}