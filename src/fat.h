#ifndef FAT_H
#define FAT_H

#define IMAGE_NAME           "filesystem.img"
#define SECTOR_SIZE          512
#define OEM_LEN              8

#define FSINFO_LEAD          0x41615252
#define FSINFO_MID           0x61417272
#define FSINFO_TRAIL         0xAA550000

#define EOC                  0xffffff8

#define ENTRY_SIZE           32
#define FILENAME_LEN         8
#define EXTENSION_LEN        3
#define ATTR_OFFSET          11

#define LFN                  0x0F
#define DIRECTORY            0x10
#define ARCHIVE              0x20

#define EMPTY_TIME           (fat_entry_time_t) {.hour = 0, .minutes=0, .seconds=0}
#define EMPTY_DATE           (fat_entry_date_t) {.year = 0, .month=0, .day=0}

#include <stdint.h>
#include <stddef.h>
#include <stdio.h> 

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
    uint8_t sector_per_cluster;
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
    uint8_t year;
    uint8_t month;
    uint8_t day;
} fat_entry_date_t;

typedef struct 
{
    uint8_t hour;
    uint8_t minutes;
    uint8_t seconds;
} fat_entry_time_t;

typedef struct
{
    char *name;
    uint8_t attributes;
    fat_entry_time_t creation_time;
    fat_entry_date_t creation_date;
    fat_entry_date_t last_access_date;
    uint32_t start_cluster;
    fat_entry_time_t modification_time;
    fat_entry_date_t modification_date;
    uint32_t size; // In bytes
} fat_entry_t;

typedef struct
{
    fat_entry_t *info;
    fat_fs_t *fs;
    uint32_t cluster;
    uint32_t cluster_cache;
    uint8_t *cache;
    uint8_t eof;
} fat_file_t;

typedef struct
{
    fat_fs_t *fs;
    fat_entry_t **entries;
    size_t max_entries;
    size_t entry_count;
} fat_dir_t;

void read_sectors(FILE *image, uint8_t n, uint32_t lba, void *buffer);
void cache_refresh(void *cache, size_t cache_size);

fat_drive_t *fat_drive_init(FILE *image, fat_bpb_t *params);
void fat_drive_fini(fat_drive_t *drive);

fat_fs_t *fat_fs_init(FILE *image);
void fat_fs_fini(fat_fs_t *fs);

void fat_bpb_read(fat_bpb_t *bpb, FILE *image);
uint8_t fsinfo_read(fat_fsinfo_t *fsinfo, fat_bpb_t *bpb, FILE *image);

fat_t *fat_init(fat_bpb_t *fat_info, fat_drive_t *drive);
uint32_t fat_readl(fat_t *fat, uint32_t offset);
void fat_cache_change(fat_t *fat, uint32_t new_lba);
void fat_fini(fat_t *fat);

void read_cluster(fat_file_t *file, uint32_t cluster, void *buffer);
uint32_t cluster_chain_read(fat_t *fat, uint32_t start, uint32_t pos);

fat_file_t *fat_file_init(fat_fs_t *fs, fat_entry_t *entry);
fat_file_t *fat_file_open(fat_fs_t *fs, char *path);
fat_file_t *fat_file_open_recursive(fat_dir_t *dir);
void fat_file_cache_change(fat_file_t *file, size_t cache_size, uint32_t new_cluster);
void fat_file_buffered_readb(fat_file_t *file, uint32_t offset, uint8_t *buffer, size_t buffer_size);
uint16_t fat_file_readl(fat_file_t *file, uint32_t offset);
uint16_t fat_file_readw(fat_file_t *file, uint32_t offset);
uint8_t fat_file_readb(fat_file_t *file, uint32_t offset);
void fat_file_close(fat_file_t *file);

fat_entry_t *fat_entry_init(fat_file_t *dir, uint32_t dir_offset);
fat_entry_t *root_entry_init(fat_fs_t *fs);
fat_entry_t *fat_entry_copy(fat_entry_t *entry);
void fat_entry_fini(fat_entry_t *entry);
size_t fat_dir_entry_get_size(fat_fs_t *fs, fat_entry_t *dir_entry);

fat_dir_t *fat_dir_alloc(fat_file_t *dir_file);
fat_dir_t *fat_dir_open(fat_file_t *dir);
void fat_dir_close(fat_dir_t *dir);

char *strtok_path(char *path, uint8_t *is_dir);

#endif