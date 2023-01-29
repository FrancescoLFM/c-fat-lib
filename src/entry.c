#include <src/fat.h>
#include <string.h>
#include <stdlib.h>

#define NAME_OFFSET          0
#define NAME_LEN             11
#define ATTR_OFFSET          11
#define CREATET_OFFSET       14
#define CREATED_OFFSET       16
#define ACCESS_OFFSET        18
#define HIGH_CLUSTER_OFFSET  20
#define LOW_CLUSTER_OFFSET   26
#define MODIFICATIONT_OFFSET 22
#define MODIFICATIOND_OFFSET 24
#define SIZE_OFFSET          28

char *shortname_to_str(uint8_t *shortname)
{
    int i = 0;
    char *file_name = malloc(sizeof(char) * (NAME_LEN+2)); // Add 2 for . and null terminator
    
    for (i=0; i < FILENAME_LEN && shortname[i] != ' '; i++)
        file_name[i] = shortname[i];
    
    if (shortname[FILENAME_LEN] != ' ') {
        file_name[i++] = '.';
    }

    for (int j=FILENAME_LEN; j < NAME_LEN && shortname[j] != ' '; j++, i++)
        file_name[i] = shortname[j];

    file_name[i] = '\0';
    return file_name;
}

void fat_entry_read_time(fat_entry_time_t *entry_time, fat_file_t *dir, uint32_t offset)
{
    uint16_t raw_time;

    raw_time = fat_file_readw(dir, offset);

    entry_time->hour = (raw_time >> 11) & 0x1F;
    entry_time->minutes = (raw_time >> 5) & 0x3F;
    entry_time->seconds = (raw_time & 0x1F) * 2;
}

void fat_entry_read_date(fat_entry_date_t *entry_date, fat_file_t *dir, uint32_t offset)
{
    uint16_t raw_date;

    raw_date = fat_file_readw(dir, offset);

    entry_date->year = (raw_date >> 9) & 0x7F;
    entry_date->month = (raw_date >> 5) & 0x0F;
    entry_date->day = raw_date & 0x1F;
}

void fat_entry_read_name(fat_entry_t *entry, fat_file_t *dir, uint32_t dir_offset)
{
    uint8_t name[NAME_LEN];

    fat_file_buffered_readb(dir, NAME_OFFSET + dir_offset, name, NAME_LEN);
    entry->name = shortname_to_str(name);

    return;
}

void fat_entry_read_start_cluster(fat_entry_t *entry, fat_file_t *dir, uint32_t dir_offset)
{
    entry->start_cluster = (
                            fat_file_readw(dir, LOW_CLUSTER_OFFSET + dir_offset)      |
                            fat_file_readw(dir, HIGH_CLUSTER_OFFSET + dir_offset) << 16
                           );
    
    return;
}

void fat_entry_read_last_access(fat_entry_t *entry, fat_file_t *dir, uint32_t dir_offset)
{
    fat_entry_read_date(&entry->creation_date, dir, ACCESS_OFFSET + dir_offset);

    return;
}

void fat_entry_read_creation(fat_entry_t *entry, fat_file_t *dir, uint32_t dir_offset)
{
    fat_entry_read_time(&entry->creation_time, dir, CREATET_OFFSET + dir_offset);
    fat_entry_read_date(&entry->creation_date, dir, CREATED_OFFSET + dir_offset);

    return;
}

void fat_entry_read_modification(fat_entry_t *entry, fat_file_t *dir, uint32_t dir_offset)
{
    fat_entry_read_time(&entry->modification_time, dir, MODIFICATIONT_OFFSET + dir_offset);
    fat_entry_read_date(&entry->modification_date, dir, MODIFICATIOND_OFFSET + dir_offset);

    return;
}

fat_entry_t *fat_entry_init(fat_file_t *dir, uint32_t dir_offset)
{
    fat_entry_t *file_entry = malloc(sizeof(fat_entry_t));

    fat_entry_read_name(file_entry, dir, dir_offset);
    file_entry->attributes = fat_file_readb(dir, ATTR_OFFSET + dir_offset);
    fat_entry_read_creation(file_entry, dir, dir_offset);
    fat_entry_read_last_access(file_entry, dir, dir_offset);

    fat_entry_read_modification(file_entry, dir, dir_offset);
    fat_entry_read_start_cluster(file_entry, dir, dir_offset);
    file_entry->size = fat_file_readl(dir, SIZE_OFFSET + dir_offset);

    return file_entry;
}

fat_entry_t *fat_entry_copy(fat_entry_t *entry)
{
    fat_entry_t *copy;

    copy = malloc(sizeof(*copy));
    if (copy == NULL)
        return NULL;
    
    memcpy(copy, entry, sizeof(*copy));
    copy->name = strdup(entry->name);
    if (copy->name == NULL) {
        free(copy);
        return NULL;
    }

    return copy;
}

size_t fat_dir_entry_get_size(fat_fs_t *fs, fat_entry_t *dir_entry)
{
    size_t size = 0;
    uint32_t ring = 0;

    while (ring < EOC)
        ring = cluster_chain_read(fs->fat, dir_entry->start_cluster, size++);
    
    return size;
}

void fat_entry_fini(fat_entry_t *entry)
{
    free(entry->name);
    free(entry);
}