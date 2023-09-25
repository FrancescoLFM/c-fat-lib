#include <include/fat.h>
#include <stdlib.h>
#include <string.h>

#define TOLOWER(C)   ((C >= 'A' && C <= 'Z') ? C + 32 : C) 

dir_t *dir_init(fat_fs_t *fs, entry_t *entry)
{
    dir_t *dir;
    size_t raw_size = cluster_chain_get_len(fs, WORDS_TO_LONG(entry->high_cluster, entry->low_cluster)) * fs->volume->cluster_sizeb;

    dir = malloc(sizeof(*dir));
    if (dir == NULL) {
        puts("Malloc error: not enough space to allocate directory");
        return NULL;
    }

    entry->size = raw_size;
    dir->ident = file_open(fs, entry);
    if (dir->ident == NULL) {
        free(dir);
        return NULL;
    }

    dir->size = raw_size / sizeof(*entry);
    dir->entries = calloc(dir->size, sizeof(*entry) * raw_size);
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
    ret->attr = raw_entry[ATTR_OFFSET];
    ret->low_cluster = BYTES_TO_WORD(raw_entry, LOW_CLUSTER_OFFSET);
    ret->ctime = BYTES_TO_LONG(raw_entry, CTIME_OFFSET);
    ret->atime = BYTES_TO_LONG(raw_entry, ATIME_OFFSET);
    ret->high_cluster = BYTES_TO_LONG(raw_entry, HIGH_CLUSTER_OFFSET);
    ret->mtime = BYTES_TO_LONG(raw_entry, MTIME_OFFSET);
    ret->size = BYTES_TO_LONG(raw_entry, SIZE_OFFSET);

    free(raw_entry);
    return ret;
}

void dir_scan(fat_fs_t *fs, dir_t *dir)
{
    size_t offset = 0;
    uint8_t entry_attr;
    entry_t *temp_entry;

    for (size_t i=0; offset < dir->ident->entry->size;) {
        entry_attr = file_readb(dir->ident, fs, offset + ATTR_OFFSET);
        if (entry_attr == LFN_ATTR)
            offset += sizeof(entry_t);
        if (entry_attr != 0) {
            temp_entry = dir_read_entry(fs, dir, offset);
            if (temp_entry->short_name[0] != INVALID_ENTRY)
                dir->entries[i++] = temp_entry;
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
    entry_t *ret;

    do {
        if(dir->entries[i] != NULL &&
            !compare_short_name(dir->entries[i]->short_name, name)) {
            ret = malloc(sizeof(*ret));
            memcpy(ret, dir->entries[i], sizeof(*ret));
            return ret;
        }
    } while(++i < dir->size);

    return NULL;
}

entry_t *dir_search_path(fat_fs_t *fs, dir_t *dir, char *path)
{
    char filename[12];
    dir_t *curr_dir = dir;
    size_t i = 0;
    entry_t *entry = NULL;
    entry_t *ret = NULL;

    // Get filename
    while (*path != '/' && i < 11 && *path) {
        filename[i++] = *path++;
    }
    filename[i] = '\0';

    entry = dir_search(curr_dir, filename);
    if (entry != NULL) {
        if (*path == '/') {
            curr_dir = dir_init(fs, entry);
            dir_scan(fs, curr_dir);
            ret = dir_search_path(fs, curr_dir, ++path);
            dir_close(fs, curr_dir);
            free(entry);
        }
        if (*path == '\0') {
            return entry;
        }
    }

    return ret;
}

void dir_close(fat_fs_t *fs, dir_t *dir)
{
    for (size_t i=0; i < dir->size; i++)
        free(dir->entries[i]);
    free(dir->entries);

    file_close(fs, dir->ident);
    free(dir);
}