#include <include/fat.h>
#include <stdlib.h>

dir_t *dir_init(fat_fs_t *fs, entry_t *entry)
{
    dir_t *dir;

    dir = malloc(sizeof(*dir));
    if (dir == NULL) {
        puts("Malloc error: not enough space to allocate directory");
        return NULL;
    }

    dir->ident = file_open(fs, entry);
    if (dir->ident == NULL) {
        free(dir);
        return NULL;
    }
    dir->size = cluster_chain_get_len(fs, WORDS_TO_LONG(entry->high_cluster, entry->low_cluster));
    dir->entries = malloc(dir->size * sizeof(entry));
    if (dir->entries == NULL) {
        puts("Malloc error: not enough space to allocate directory entries");
        file_close(fs, dir->ident);
        free(dir);
        return NULL;
    }

    return dir;
}

