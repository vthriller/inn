/*  $Id$
**
**  Group index handling for the tradindexed overview method.
**
**  Implements the handling of the group.index file for the tradindexed
**  overview method.  This file contains an entry for every group and stores
**  the high and low article marks and the base article numbers for each
**  individual group index file.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libinn.h"
#include "ov.h"
#include "private.h"
#include "structure.h"

#ifndef NFSREADER
# define NFSREADER 0
#endif

/* Returned to callers as an opaque data type, this stashes all of the
   information about an open group.index file. */
struct group_index {
    char *path;
    int fd;
    int mode;
    struct group_header *header;
    struct group_entry *entries;
    int count;
};

/* The value used to mark an invalid or empty location in the group index. */
static const GROUPLOC empty_loc = { -1 };

/* Internal prototypes. */
static int group_index_entry_count(size_t size);
static size_t group_index_file_size(int count);
static bool group_index_map(struct group_index *);
static bool group_index_maybe_remap(struct group_index *, GROUPLOC loc);
static bool group_index_expand(struct group_index *);
static GROUPLOC group_index_find(struct group_index *, const char *group);

/*
**  Given a file size, return the number of group entries that it contains.
*/
static int
group_index_entry_count(size_t size)
{
    return (size - sizeof(struct group_header)) / sizeof(struct group_entry);
}

/*
**  Given a number of group entries, return the required file size.
*/
static size_t
group_index_file_size(int count)
{
    return sizeof(struct group_header) + count * sizeof(struct group_entry);
}

/*
**  Memory map (or read into memory) the key portions of the group.index
**  file.  Takes a struct group_index to fill in and returns true on success
**  and false on failure.
*/
static bool
group_index_map(struct group_index *index)
{
    if (NFSREADER && (index->mode & OV_WRITE)) {
        warn("tradindexed: cannot open for writing without mmap");
        return false;
    }

    if (NFSREADER) {
        ssize_t header_size;
        ssize_t entry_size;

        header_size = sizeof(struct group_header);
        entry_size = index->count * sizeof(struct group_entry);
        index->header = xmalloc(header_size);
        index->entries = xmalloc(entry_size);
        if (read(index->fd, index->header, header_size) != header_size) {
            syswarn("tradindexed: cannot read header from %s", index->path);
            goto fail;
        }
        if (read(index->fd, index->entries, entry_size) != entry_size) {
            syswarn("tradindexed: cannot read entries from %s", index->path);
            goto fail;
        }
        return true;

    fail:
        free(index->header);
        free(index->entries);
        return false;

    } else {
        char *data;
        size_t size;
        int flag = PROT_READ;

        if (index->mode & OV_WRITE)
            flag = PROT_READ | PROT_WRITE;
        size = group_index_file_size(index->count);
        data = mmap(NULL, size, flag, MAP_SHARED, index->fd, 0);
        if (data == MAP_FAILED) {
            syswarn("tradindexed: cannot mmap %s", index->path);
            return false;
        }
        index->header = (struct group_header *)(void *) data;
        index->entries = (struct group_entry *)
            (void *)(data + sizeof(struct group_header));
        return true;
    }
}

/*
**  Given a group location, remap the index file if our existing mapping isn't
**  large enough to include that group.  (This can be the case when another
**  writer is appending entries to the group index.)  Not yet implemented.
*/
static bool
group_index_maybe_remap(struct group_index *index UNUSED, GROUPLOC loc UNUSED)
{
    return true;
}

/*
**  Expand the group.index file to hold more entries; also used to build the
**  initial file.  Not yet implemented.
*/
static bool
group_index_expand(struct group_index *index UNUSED)
{
    return false;
}

/*
**  Open the group.index file and allocate a new struct for it, returning a
**  pointer to that struct.  Takes the overview open mode, which is some
**  combination of OV_READ and OV_WRITE.
*/
struct group_index *
group_index_open(int mode)
{
    struct group_index *index;
    int open_mode;
    struct stat st;

    index = xmalloc(sizeof(struct group_index));
    index->path = concatpath(innconf->pathoverview, "group.index");
    index->mode = mode;
    index->header = NULL;
    open_mode = (mode & OV_WRITE) ? O_RDWR | O_CREAT : O_RDONLY;
    index->fd = open(index->path, open_mode, ARTFILE_MODE);
    if (index->fd < 0) {
        syswarn("tradindexed: cannot open %s", index->path);
        goto fail;
    }

    if (fstat(index->fd, &st) < 0) {
        syswarn("tradindexed: cannot fstat %s", index->path);
        goto fail;
    }
    if (st.st_size > sizeof(struct group_header)) {
        index->count = group_index_entry_count(st.st_size);
        if (!group_index_map(index))
            goto fail;
    } else {
        index->count = 0;
        if (mode & OV_WRITE) {
            if (!group_index_expand(index))
                goto fail;
        } else {
            index->count = 0;
            index->header = NULL;
            index->entries = NULL;
        }
    }
    close_on_exec(index->fd, true);
    return index;

 fail:
    group_index_close(index);
    return NULL;
}

/*
**  Find a group in the index file, returning the GROUPLOC for that group.
*/
static GROUPLOC
group_index_find(struct group_index *index, const char *group)
{
    HASH grouphash;
    unsigned int bucket;
    GROUPLOC loc;

    grouphash = Hash(group, strlen(group));
    memcpy(&bucket, &grouphash, sizeof(index));
    loc = index->header->hash[bucket % GROUPHEADERHASHSIZE];
    if (!group_index_maybe_remap(index, loc))
        return empty_loc;

    while (loc.recno >= 0) {
        struct group_entry *entry;

        entry = index->entries + loc.recno;
        if (entry->deleted == 0)
            if (memcmp(&grouphash, &entry->hash, sizeof(grouphash)) == 0)
                return loc;
        loc = entry->next;
    }
    return empty_loc;
}

/*
**  Return the information stored about a given group in the group index.
*/
bool
group_index_info(struct group_index *index, const char *group,
                 unsigned long *high, unsigned long *low,
                 unsigned long *count, char *flag)
{
    GROUPLOC loc;

    loc = group_index_find(index, group);
    if (loc.recno == empty_loc.recno)
        return false;

    if (high != NULL)
        *high = index->entries[loc.recno].high;
    if (low != NULL)
        *low = index->entries[loc.recno].low;
    if (count != NULL)
        *count = index->entries[loc.recno].count;
    if (flag != NULL)
        *flag = index->entries[loc.recno].flag;
    return true;
}

/*
**  Dump the complete contents of the group.index file in human-readable form
**  to stdout.  The format is:
**
**    hash high low base count flag deleted
**
**  all on one line for each group contained in the index file.  Hash is the
**  textual representation of the MD5 hash of the group; high, low, base,
**  count, and deleted are numbers; and flag is a character.
*/
void
group_index_dump(struct group_index *index)
{
    int bucket;
    GROUPLOC current;
    struct group_entry *entry;

    for (bucket = 0; bucket < GROUPHEADERHASHSIZE; bucket++) {
        current = index->header->hash[bucket];
        while (current.recno != empty_loc.recno) {
            if (!group_index_maybe_remap(index, current)) {
                warn("tradindexed: cannot remap %s", index->path);
                return;
            }
            entry = index->entries + current.recno;
            printf("%s %lu %lu %lu %lu %c %lu %lu\n", HashToText(entry->hash),
                   entry->high, entry->low, entry->base,
                   (unsigned long) entry->count, entry->flag, entry->deleted);
            free(hash);
            current = entry->next;
        }
    }
}

/*
**  Close an open handle to the group index file, freeing the group_index
**  structure at the same time.  The argument to this function becomes invalid
**  after this call.
*/
void
group_index_close(struct group_index *index)
{
    if (index->header != NULL) {
        if (NFSREADER) {
            free(index->header);
            free(index->entries);
        } else {
            size_t count;

            count = group_index_file_size(index->count);
            if (munmap((caddr_t) index->header, count) < 0)
                warn("tradindexed: cannot munmap %s", index->path);
        }
    }
    if (index->fd >= 0)
        close(index->fd);
    free(index->path);
    free(index);
}
