#include <include/fat.h>
#include <stdlib.h>
#include <string.h>

dir_t *dir_init(fat_fs_t *fs, entry_t *entry)
{
    dir_t *dir;
    size_t raw_size;

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
    raw_size = cluster_chain_get_len(fs, WORDS_TO_LONG(entry->high_cluster, entry->low_cluster)) * fs->volume->cluster_sizeb;
    dir->size = raw_size / sizeof(*entry);
    dir->entries = malloc(raw_size);
    if (dir->entries == NULL) {
        puts("Malloc error: not enough space to allocate directory entries");
        file_close(fs, dir->ident);
        free(dir);
        return NULL;
    }

    return dir;
}

entry_t *dir_read_entry(fat_fs_t *fs, dir_t *dir, size_t offset)
{
    uint8_t *raw_entry;
    entry_t *ret;

    ret = malloc(sizeof(*ret));
    memset(ret, 0, sizeof(*ret));
    raw_entry = file_read(dir->ident, fs, offset, sizeof(*ret));
    strncpy(ret->short_name, (char *) raw_entry, SHORT_NAME_LEN);
    
    return ret;
}

void dir_scan(fat_fs_t *fs, dir_t *dir)
{
    size_t offset = 0;
    uint8_t entry_attr;

    for (size_t i=0; i < dir->size;) {
        entry_attr = file_readb(dir->ident, fs, offset + ATTR_OFFSET);
        if (entry_attr == LFN_ATTR) {
            offset += sizeof(entry_t);
            dir->entries[i] = dir_read_entry(fs, dir, offset);
            i++;
        }

        else if (entry_attr != 0) {
            dir->entries[i] = dir_read_entry(fs, dir, offset);
            i++;
        }
        
        offset += sizeof(entry_t);
    }
}
