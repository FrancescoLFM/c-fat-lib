#ifndef FAT_H
#define FAT_H

#include <stdio.h>
#include <stdint.h>

#include <include/cache.h>

#define DRIVENAME                    "filesystem.img"

#define LABEL_LENGTH                 11
#define SECTOR_SIZE                  512
#define UNDEFINED_SECCOUNT           65535

// Macros
#define BYTES_TO_WORD(BYTES, OFF)    (BYTES[0 + OFF] + (BYTES[1 + OFF] << 8))
#define BYTES_TO_LONG(BYTES, OFF)    (BYTES[0 + OFF] + (BYTES[1 + OFF] << 8) + (BYTES[2 + OFF] << 16) + (BYTES[3 + OFF] << 24))

#define BPB_SECTOR              0

#define BYTES_PER_SECTOR        0x0B
#define SECTOR_PER_CLUSTER      0x0D
#define RESERVED_SECTORS        0x0E
#define FAT_TABLE_COUNT         0x10
#define SECTOR_COUNT            0x13
#define LARGE_SECTOR_COUNT      0x20
#define FAT_TABLE_SIZE          0x24
#define ROOT_CLUSTER            0x2C
#define FSINFO_SECTOR           0x30

#define LEAD_SIGNATURE1_OFF     0x00
#define LEAD_SIGNATURE1         0x41615252
#define LEAD_SIGNATURE2_OFF     0x1E4
#define LEAD_SIGNATURE2         0x61417272
#define FREE_CLUSTER_COUNT      0x1E8
#define FREE_CLUSTER            0x1EC
#define UNKNOWN_FREE_CLUSTER    0xFFFFFFFF

#define FAT_READ                0
#define FAT_WRITE               1

#define FS_ERROR                1
#define READ_ERROR              0xFFFFFFFF
#define CLUSTER_ALLOC_ERR       1

#define EOC1                    0xFFFFFF8
#define EOC2                    0xFFFFFFF

typedef struct fat_fsinfo fat_fsinfo_t;
typedef struct fat_volume fat_volume_t;
typedef struct fat_table fat_table_t;
typedef struct fat_fs fat_fs_t;
typedef struct cache cache_t;

struct fat_volume
{
    FILE *drive;
    char *label;
    uint32_t sector_count;
    uint32_t cluster_count;
    size_t sector_size;
    size_t cluster_size; // In sectors
};

struct fat_fsinfo
{
    uint32_t sector;
    uint8_t *buffer;
    uint32_t root_cluster;
    uint32_t free_cluster_count;
    uint32_t free_cluster;
};

struct fat_table
{
    uint32_t address;
    size_t size;
    uint32_t count;
    cache_t *cache;
};

struct fat_fs
{
    fat_volume_t *volume;
    fat_table_t *table;
    fat_fsinfo_t info;
};

// src/cache.c
fat_volume_t *fat_volume_init(FILE *drive);
uint8_t *read_sector(fat_volume_t *volume, uint32_t lba);
void write_sector(fat_volume_t *volume, uint32_t lba, uint8_t *buffer);
void fat_volume_fini(fat_volume_t *volume);

// src/fs.c
fat_fs_t *fat_fs_init(FILE *partition);
uint8_t fat_fs_getinfo(fat_fs_t *fs);
void fat_fs_fini(fat_fs_t *fs);

// src/table.c
fat_table_t *fat_table_init(fat_volume_t *volume);
void fat_table_fini(fat_table_t *table, fat_volume_t *volume);
uint32_t fat_table_read(fat_fs_t *fs, uint32_t cluster);
uint32_t free_cluster_count_read(fat_fs_t *fs);
uint32_t first_free_cluster_read(fat_fs_t *fs);
uint32_t fat_table_alloc_cluster(fat_fs_t *fs, uint32_t content);

#endif