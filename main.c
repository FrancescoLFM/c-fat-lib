#include <stdio.h>
#include <include/fat.h>

void fat_fs_printinfo(fat_fs_t *fs)
{
    puts("-------------------------------");
    printf("Volume label:\t\t%s\n", fs->volume->label);
    printf("Sector count:\t\t%u\n", fs->volume->sector_count);
    printf("Sector size:\t\t%luB\n", fs->volume->sector_size);
    printf("Cluster size:\t\t%lu Sec\n\n", fs->volume->cluster_size);
    printf("Fat table address:\t%u\n", fs->table->address);
    printf("Fat table size:\t\t%lu Sec\n", fs->table->size);
    printf("Fat table count:\t%u FATs\n", fs->table->count);
    if (fs->table->cache != NULL)
        puts("Fat table cache ENGAGED!");
    printf("\nRoot cluster:\t\t%u\n", fs->info.root_cluster);
    printf("Free cluster count:\t%u\n", fs->info.free_cluster_count);
    printf("First free cluster:\t%u\n", fs->info.free_cluster);
    puts("-------------------------------");
}

int main() 
{
    FILE *partition;
    fat_fs_t *fs;
    uint8_t err = 0;

    partition = fopen(DRIVENAME, "r");
    fs = fat_fs_init(partition);
    if (fs == NULL) {
        puts("Filesystem error: failed to initiate filesystem");
        goto exit;
    }

    puts("Filesystem initiated.");
    fat_fs_printinfo(fs);

    printf("%d\n", free_cluster_count_read(fs));

    fat_fs_fini(fs);

exit:
    fclose(partition);
    return err;
}