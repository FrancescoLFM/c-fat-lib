#include <include/fat.h>
#include <stdlib.h>
#include <string.h>

#define TOLOWER(C)   ((C >= 'A' && C <= 'Z') ? C + 32 : C) 

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

    free(raw_entry);
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

int strcmp_insensitive(char *a, char *b)
{
    size_t i = 0;

    do {
        if (!a[i] || !b[i])
            return a[i]-b[i];
        i++;
    } while (TOLOWER(a[i]) == TOLOWER(b[i]));

    return a[i] - b[i];
}

int compare_short_name(char* name, char* str) {
    int i;
    char short_name_str[12];

    struct short_name *short_name = (struct short_name *) name;

    for (i = 0; i < 8 && short_name->name[i] != ' '; i++) {
        short_name_str[i] = short_name->name[i];
    }

    // Se c'è un'estensione, aggiungila con un '.'
    if (short_name->ext[0] != ' ') {
        short_name_str[i++] = '.';
        int j;
        for (j = 0; j < 3 && short_name->ext[j] != ' '; j++, i++) {
            short_name_str[i] = short_name->ext[j];
        }
    }

    // Aggiungi il terminatore di stringa
    short_name_str[i] = '\0';

    // Confronta la stringa temporanea con la stringa passata come argomento
    return strcmp_insensitive(short_name_str, str);
}

entry_t *dir_search(dir_t *dir, char *name)
{
    size_t i = 0;

    do {
        if(!compare_short_name(dir->entries[i]->short_name, name))
            return dir->entries[i];
    } while(++i < dir->size);

    return NULL;
}

void dir_close(fat_fs_t *fs, dir_t *dir)
{
    for (size_t i=0; i < dir->size; i++)
        free(dir->entries[i]);
    free(dir->entries);

    file_close(fs, dir->ident);
    free(dir);
}