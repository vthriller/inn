/*  $Id$
**
**  Indexed overview method.
*/
#include "config.h"
#include "clibrary.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include "libinn.h"
#include "macros.h"
#include "ov.h"
#include "paths.h"
#include "ov.h"
#include "ovinterface.h"
#include "tradindexed.h"
#include "storage.h"

/* Data structure for specifying a location in the group index */
typedef struct {
    int                 recno;             /* Record number in group index */
} GROUPLOC;

#define GROUPHEADERHASHSIZE (16 * 1024)
#define GROUPHEADERMAGIC    (~(0xf1f0f33d))

typedef struct {
    int                 magic;
    GROUPLOC            hash[GROUPHEADERHASHSIZE];
    GROUPLOC            freelist;
} GROUPHEADER;

/* The group is matched based on the MD5 of the grouname. This may prove to
   be inadequate in the future, if so, the right thing to do is to is
   probably just to add a SHA1 hash into here also.  We get a really nice
   benefit from this being fixed length, we should try to keep it that way.
*/
typedef struct {
    HASH                hash;             /* MD5 hash of the group name, if */
    HASH                alias;            /* If not empty then this is the hash
					     of the group that this group is an
					     alias for */
    ARTNUM              high;             /* High water mark in group */
    ARTNUM              low;              /* Low water mark in group */
    ARTNUM              base;             /* Article number of the first entry
					     in the index */
    int                 count;            /* Number of articles in group */
    int                 flag;             /* Posting/Moderation Status */
    time_t              deleted;          /* When this was deleted, 0 otherwise */    
    ino_t               indexinode;       /* inode of the index file for the group,
					     used to detect when the file has been
					     recreated and swapped out. */
    GROUPLOC            next;             /* Next block in this chain */
} GROUPENTRY;

typedef struct {
    OFFSET_T           offset;
    int                length;
    time_t             arrived;
    time_t             expires;
    TOKEN              token;
} INDEXENTRY;

typedef struct {
    HASH               hash;
    char               *group;
    GROUPLOC           gloc;
    int                indexfd;
    int                datafd;
    INDEXENTRY         *indexmem;
    char               *datamem;
    int                indexlen;
    int                datalen;
    ino_t              indexinode;
} GROUPHANDLE;

typedef struct {
    char                *group;
    int                 limit;
    int                 base;
    int                 cur;
    GROUPHANDLE         *gh;
} OV3SEARCH;

typedef struct _CR {
    GROUPHANDLE        *gh;
    time_t             lastused;      /* time this was last used */
    int                refcount;      /* Number of things currently using this */
    struct _CR         *next;
} CACHEENTRY;

/*
**  A buffer; re-uses space.
*/
typedef struct _BUFFER {
    int		Used;
    int		Size;
    char	*Data;
} BUFFER;

#define CACHETABLESIZE 128
#define MAXCACHETIME (60*5)

STATIC int              GROUPfd;
STATIC GROUPHEADER      *GROUPheader = NULL;
STATIC GROUPENTRY       *GROUPentries = NULL;
STATIC int              GROUPcount = 0;
STATIC GROUPLOC         GROUPemptyloc = { -1 };

STATIC CACHEENTRY *CACHEdata[CACHETABLESIZE];
STATIC int OV3mode;
STATIC int OV3padamount = 128;
STATIC int CACHEentries = 0;
STATIC int CACHEhit = 0;
STATIC int CACHEmiss = 0;
STATIC int CACHEmaxentries = 128;
STATIC BOOL Cutofflow;

STATIC GROUPLOC GROUPnewnode(void);
STATIC BOOL GROUPremapifneeded(GROUPLOC loc);
STATIC void GROUPLOCclear(GROUPLOC *loc);
STATIC BOOL GROUPLOCempty(GROUPLOC loc);
STATIC BOOL GROUPlockhash(LOCKTYPE type);
STATIC BOOL GROUPlock(GROUPLOC gloc, LOCKTYPE type);
STATIC BOOL GROUPfilesize(int count);
STATIC BOOL GROUPexpand(int mode);
STATIC BOOL OV3packgroup(char *group, int delta);
STATIC GROUPHANDLE *OV3opengroup(char *group, BOOL needcache);
STATIC void OV3cleancache(void);
STATIC BOOL OV3addrec(GROUPENTRY *ge, GROUPHANDLE *gh, int artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires);
STATIC BOOL OV3closegroup(GROUPHANDLE *gh, BOOL needcache);
STATIC void OV3getdirpath(char *group, char *path);
STATIC void OV3getIDXfilename(char *group, char *path);
STATIC void OV3getDATfilename(char *group, char *path);

BOOL tradindexed_open(int mode) {
    char                dirname[1024];
    char                *groupfn;
    struct stat         sb;
    int                 fdlimit;
    int                 flag = 0;

    OV3mode = mode;
    if (OV3mode & OV_READ)
	CACHEmaxentries = 1;
    else
	CACHEmaxentries = innconf->overcachesize;
    fdlimit = getfdlimit();
    if (fdlimit > 0 && fdlimit < CACHEmaxentries * 2) {
        syslog(L_FATAL, "tradindexed: not enough file descriptors for"
               " overview cache, increase rlimitnofile or decrease"
               " overcachesize");
        return FALSE;
    }
    memset(&CACHEdata, '\0', sizeof(CACHEdata));
    
    strcpy(dirname, innconf->pathoverview);
    groupfn = NEW(char, strlen(dirname) + strlen("/group.index") + 1);
    strcpy(groupfn, dirname);
    strcat(groupfn, "/group.index");
    GROUPfd = open(groupfn, O_RDWR | O_CREAT, ARTFILE_MODE);
    if (GROUPfd < 0) {
	syslog(L_FATAL, "tradindexed: could not create %s: %m", groupfn);
	DISPOSE(groupfn);
	return FALSE;
    }
    
    if (fstat(GROUPfd, &sb) < 0) {
	syslog(L_FATAL, "tradindexed: could not fstat %s: %m", groupfn);
	DISPOSE(groupfn);
	close(GROUPfd);
	return FALSE;
    }
    if (sb.st_size > sizeof(GROUPHEADER)) {
	if (mode & OV_READ)
	    flag |= PROT_READ;
	if (mode & OV_WRITE) {
	    /* 
	     * Note: below mapping of groupheader won't work unless we have 
	     * both PROT_READ and PROT_WRITE perms.
	     */
	    flag |= PROT_WRITE|PROT_READ;
	}
	GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
	if ((GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount), flag,
					       MAP_SHARED, GROUPfd, 0)) == (GROUPHEADER *) -1) {
	    syslog(L_FATAL, "tradindexed: could not mmap %s in tradindexed_open: %m", groupfn);
	    DISPOSE(groupfn);
	    close(GROUPfd);
	    return FALSE;
	}
	GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    } else {
	GROUPcount = 0;
	if (!GROUPexpand(mode)) {
	    DISPOSE(groupfn);
	    close(GROUPfd);
	    return FALSE;
	}
    }
    CloseOnExec(GROUPfd, 1);
    
    DISPOSE(groupfn);
    Cutofflow = FALSE;

    return TRUE;
}

STATIC GROUPLOC GROUPfind(char *group) {
    HASH                grouphash;
    unsigned int        i;
    GROUPLOC            loc;
    
    grouphash = Hash(group, strlen(group));
    memcpy(&i, &grouphash, sizeof(i));

    loc = GROUPheader->hash[i % GROUPHEADERHASHSIZE];
    GROUPremapifneeded(loc);

    while (!GROUPLOCempty(loc)) {
	if (GROUPentries[loc.recno].deleted == 0) {
	    if (memcmp(&grouphash, &GROUPentries[loc.recno].hash, sizeof(HASH)) == 0) {
		return loc;
	    }
	}
	loc = GROUPentries[loc.recno].next;
    }
    return GROUPemptyloc;
} 

BOOL tradindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag) {
    GROUPLOC            gloc;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc)) {
	return FALSE;
    }
    if (lo != NULL)
	*lo = GROUPentries[gloc.recno].low;
    if (hi != NULL)
	*hi = GROUPentries[gloc.recno].high;
    if (count != NULL)
	*count = GROUPentries[gloc.recno].count;
    if (flag != NULL)
	*flag = GROUPentries[gloc.recno].flag;
    return TRUE;
}

BOOL tradindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag) {
    unsigned int        i;
    HASH                grouphash;
    GROUPLOC            loc;
    GROUPENTRY          *ge;

    loc = GROUPfind(group);
    if (!GROUPLOCempty(loc)) {
	GROUPentries[loc.recno].flag = *flag;
	return TRUE;
    }
    grouphash = Hash(group, strlen(group));
    memcpy(&i, &grouphash, sizeof(i));
    i = i % GROUPHEADERHASHSIZE;
    GROUPlockhash(LOCK_WRITE);
    loc = GROUPnewnode();
    ge = &GROUPentries[loc.recno];
    ge->hash = grouphash;
    if (lo != 0)
	ge->low = lo;
    ge->high = hi;
    ge->deleted = ge->low = ge->base = ge->count = 0;
    ge->flag = *flag;
    ge->next = GROUPheader->hash[i];
    GROUPheader->hash[i] = loc;
    GROUPlockhash(LOCK_UNLOCK);
    return TRUE;
}

STATIC BOOL GROUPfilesize(int count) {
    return (count * sizeof(GROUPENTRY)) + sizeof(GROUPHEADER);
}

/* Check if the given GROUPLOC refers to GROUPENTRY that we don't have mmap'ed,
** if so then see if the file has been grown by another writer and remmap
*/
STATIC BOOL GROUPremapifneeded(GROUPLOC loc) {
    struct stat         sb;
    
    if (loc.recno < GROUPcount)
	return TRUE;

    if (fstat(GROUPfd, &sb) < 0)
	return FALSE;

    if (GROUPfilesize(GROUPcount) >= sb.st_size)
	return TRUE;

    if (GROUPheader) {
	if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
	    syslog(L_FATAL, "tradindexed: could not munmap group.index in GROUPremapifneeded: %m");
	    return FALSE;
	}
    }

    GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
    GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
				     PROT_READ | PROT_WRITE, MAP_SHARED, GROUPfd, 0);
    if (GROUPheader == (GROUPHEADER *) -1) {
	syslog(L_FATAL, "tradindexed: could not mmap group.index in GROUPremapifneeded: %m");
	return FALSE;
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    return TRUE;
}

/* This function does not need to lock because it's callers are expected to do so */
STATIC BOOL GROUPexpand(int mode) {
    int                 i;
    int                 flag = 0;
    
    if (GROUPheader) {
	if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
	    syslog(L_FATAL, "tradindexed: could not munmap group.index in GROUPexpand: %m");
	    return FALSE;
	}
    }
    GROUPcount += 1024;
    if (ftruncate(GROUPfd, GROUPfilesize(GROUPcount)) < 0) {
	syslog(L_FATAL, "tradindexed: could not extend group.index: %m");
	return FALSE;
    }
    if (mode & OV_READ)
	flag |= PROT_READ;
    if (mode & OV_WRITE) {
	/* 
	 * Note: below check of magic won't work unless we have both PROT_READ
	 * and PROT_WRITE perms.
	 */
	flag |= PROT_WRITE|PROT_READ;
    }
    GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
				     flag, MAP_SHARED, GROUPfd, 0);
    if (GROUPheader == (GROUPHEADER *) -1) {
	syslog(L_FATAL, "tradindexed: could not mmap group.index in GROUPexpand: %m");
	return FALSE;
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    if (GROUPheader->magic != GROUPHEADERMAGIC) {
	GROUPheader->magic = GROUPHEADERMAGIC;
	GROUPLOCclear(&GROUPheader->freelist);
	for (i = 0; i < GROUPHEADERHASHSIZE; i++)
	    GROUPLOCclear(&GROUPheader->hash[i]);
    }
    /* Walk the new entries from the back to the front, adding them to the freelist */
    for (i = GROUPcount - 1; (GROUPcount - 1024) <= i; i--) {
	GROUPentries[i].next = GROUPheader->freelist;
	GROUPheader->freelist.recno = i;
    }
    return TRUE;
}

STATIC GROUPLOC GROUPnewnode(void) {
    GROUPLOC            loc;
    
    /* If we didn't find any free space, then make some */
    if (GROUPLOCempty(GROUPheader->freelist)) {
	if (!GROUPexpand(OV3mode)) {
	    return GROUPemptyloc;
	}
    }
    assert(!GROUPLOCempty(GROUPheader->freelist));
    loc = GROUPheader->freelist;
    GROUPheader->freelist = GROUPentries[GROUPheader->freelist.recno].next;
    return loc;
}

BOOL tradindexed_groupdel(char *group) {
    GROUPLOC            gloc;
    char                *sepgroup;
    char                *p;
    char                IDXpath[BIG_BUFFER];
    char                DATpath[BIG_BUFFER];
    char                **groupparts = NULL;
    int                 i, j;
    
    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return TRUE;

    GROUPentries[gloc.recno].deleted = time(NULL);
    HashClear(&GROUPentries[gloc.recno].hash);

    sepgroup = COPY(group);
    for (p = sepgroup; *p != '\0'; p++)
	if (*p == '.')
	    *p = ' ';
    
    i = argify(sepgroup, &groupparts);
    DISPOSE(sepgroup);
    strcpy(IDXpath, innconf->pathoverview);
    strcat(IDXpath, "/");
    for (p = IDXpath + strlen(IDXpath), j = 0; j < i; j++) {
	*p++ = groupparts[j][0];
	*p++ = '/';
    }
    *p = '\0';
    freeargify(&groupparts);

    sprintf(p, "%s.DAT", group);
    strcpy(DATpath, IDXpath);
    sprintf(p, "%s.IDX", group);
    unlink(IDXpath);
    unlink(DATpath);

    return TRUE;
}

STATIC void GROUPLOCclear(GROUPLOC *loc) {
    loc->recno = -1;
}

STATIC BOOL GROUPLOCempty(GROUPLOC loc) {
    return (loc.recno < 0);
}

STATIC BOOL GROUPlockhash(LOCKTYPE type) {
    return LockRange(GROUPfd, type, TRUE, 0, sizeof(GROUPHEADER));
}

STATIC BOOL GROUPlock(GROUPLOC gloc, LOCKTYPE type) {
    return LockRange(GROUPfd,
		     type,
		     TRUE,
		     sizeof(GROUPHEADER) + (sizeof(GROUPENTRY) * gloc.recno),
		     sizeof(GROUPENTRY));
}

STATIC BOOL OV3mmapgroup(GROUPHANDLE *gh) {
    struct stat         sb;
    
    if (fstat(gh->datafd, &sb) < 0) {
	syslog(L_ERROR, "tradindexed: could not fstat data file for %s: %m", gh->group);
	return FALSE;
    }
    gh->datalen = sb.st_size;
    if (fstat(gh->indexfd, &sb) < 0) {
	syslog(L_ERROR, "tradindexed: could not fstat index file for %s: %m", gh->group);
	return FALSE;
    }
    gh->indexlen = sb.st_size;
    if (gh->datalen == 0 || gh->indexlen == 0) {
	gh->datamem = (char *)-1;
	gh->indexmem = (INDEXENTRY *)-1;
	return TRUE;
    }
    if (!gh->datamem) {
	if ((gh->datamem = (char *)mmap(0, gh->datalen, PROT_READ, MAP_SHARED,
					gh->datafd, 0)) == (char *)-1) {
	    syslog(L_ERROR, "tradindexed: could not mmap data file for %s: %m", gh->group);
	    return FALSE;
	}
    }
    if (!gh->indexmem) {
	if ((gh->indexmem = (INDEXENTRY *)mmap(0, gh->indexlen, PROT_READ, MAP_SHARED, 
					       gh->indexfd, 0)) == (INDEXENTRY *)-1) {
	    syslog(L_ERROR, "tradindexed: could not mmap index file for  %s: %m", gh->group);
	    munmap(gh->datamem, gh->datalen);
	    return FALSE;
	}
    }
    return TRUE;
}

STATIC GROUPHANDLE *OV3opengroupfiles(char *group) {
    char                *sepgroup;
    char                *p;
    char                IDXpath[BIG_BUFFER];
    char                DATpath[BIG_BUFFER];
    char                **groupparts = NULL;
    GROUPHANDLE         *gh;
    int                 i, j;
    struct stat         sb;
    
    sepgroup = COPY(group);
    for (p = sepgroup; *p != '\0'; p++)
	if (*p == '.')
	    *p = ' ';
    
    i = argify(sepgroup, &groupparts);
    DISPOSE(sepgroup);
    strcpy(IDXpath, innconf->pathoverview);
    strcat(IDXpath, "/");
    for (p = IDXpath + strlen(IDXpath), j = 0; j < i; j++) {
	*p++ = groupparts[j][0];
	*p++ = '/';
    }
    *p = '\0';
    freeargify(&groupparts);

    sprintf(p, "%s.DAT", group);
    strcpy(DATpath, IDXpath);
    sprintf(p, "%s.IDX", group);

    gh = NEW(GROUPHANDLE, 1);
    memset(gh, '\0', sizeof(GROUPHANDLE));
    if ((gh->datafd = open(DATpath, O_RDWR| O_APPEND | O_CREAT, 0660)) < 0) {
	p = strrchr(IDXpath, '/');
	*p = '\0';
	if (!MakeDirectory(IDXpath, TRUE)) {
	    syslog(L_ERROR, "tradindexed: could not create directory %s", IDXpath);
	    return NULL;
	}
	*p = '/';
	if ((gh->datafd = open(DATpath, O_RDWR| O_APPEND | O_CREAT, 0660)) < 0) {
	    DISPOSE(gh);
	    if (errno == ENOENT)
		return NULL;
	    syslog(L_ERROR, "tradindexed: could not open %s: %m", DATpath);
	    return NULL;
	}
    }
    if ((gh->indexfd = open(IDXpath, O_RDWR | O_CREAT, 0660)) < 0) {
	close(gh->datafd);
	DISPOSE(gh);
	syslog(L_ERROR, "tradindexed: could not open %s: %m", IDXpath);
	return NULL;
    }
    if (fstat(gh->indexfd, &sb) < 0) {
	close(gh->datafd);
	close(gh->indexfd);
	DISPOSE(gh);
	syslog(L_ERROR, "tradindexed: could not fstat %s: %m", IDXpath);
	return NULL;
    }
    CloseOnExec(gh->datafd, 1);
    CloseOnExec(gh->indexfd, 1);
    gh->indexinode = sb.st_ino;
    gh->indexlen = gh->datalen = -1;
    gh->indexmem = NULL;
    gh->datamem = NULL;
    gh->group = COPY(group);
    return gh;
}

STATIC void OV3closegroupfiles(GROUPHANDLE *gh) {
    close(gh->indexfd);
    close(gh->datafd);
    if (gh->indexmem)
	munmap((void *)gh->indexmem, gh->indexlen);
    if (gh->datamem)
	munmap(gh->datamem, gh->datalen);
    if (gh->group)
	DISPOSE(gh->group);
    DISPOSE(gh);
}


STATIC void OV3cleancache(void) {
    int                 i;
    CACHEENTRY          *ce, *prev;
    CACHEENTRY          *saved = NULL;
    CACHEENTRY          *sprev = NULL;
    int                 savedbucket;
    
    while (CACHEentries >= CACHEmaxentries) {
	for (i = 0; i < CACHETABLESIZE; i++) {
	    for (prev = NULL, ce = CACHEdata[i]; ce != NULL; prev = ce, ce = ce->next) {
		if (ce->refcount > 0)
		    continue;
		if ((saved == NULL) || (saved->lastused > ce->lastused)) {
		    saved = ce;
		    sprev = prev;
		    savedbucket = i;
		}
	    }
	}

	if (saved != NULL) {
	    OV3closegroupfiles(saved->gh);
	    if (sprev) {
		sprev->next = saved->next;
	    } else {
		CACHEdata[savedbucket] = saved->next;
	    }
	    DISPOSE(saved);
	    CACHEentries--;
	    return;
	}
	syslog(L_NOTICE, "tradindexed: group cache is full, waiting 10 seconds");
	sleep(10);
    }
}


STATIC GROUPHANDLE *OV3opengroup(char *group, BOOL needcache) {
    unsigned int        hashbucket;
    GROUPHANDLE         *gh;
    HASH                hash;
    CACHEENTRY          *ce, *prev;
    time_t              Now;
    GROUPLOC            gloc;
    GROUPENTRY          *ge;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return NULL;

    ge = &GROUPentries[gloc.recno];
    if (needcache) {
      Now = time(NULL);
      hash = Hash(group, strlen(group));
      memcpy(&hashbucket, &hash, sizeof(hashbucket));
      hashbucket %= CACHETABLESIZE;
      for (prev = NULL, ce = CACHEdata[hashbucket]; ce != NULL; prev = ce, ce = ce->next) {
	if (memcmp(&ce->gh->hash, &hash, sizeof(hash)) == 0) {
	    if (((Now - ce->lastused) > MAXCACHETIME) ||
		(GROUPentries[gloc.recno].indexinode != ce->gh->indexinode)) {
		OV3closegroupfiles(ce->gh);
		if (prev)
		    prev->next = ce->next;
		else
		    CACHEdata[hashbucket] = ce->next;
		DISPOSE(ce);
		CACHEentries--;
		break;
	    }
	    CACHEhit++;
	    ce->lastused = Now;
	    ce->refcount++;
	    return ce->gh;
	}
      }
      OV3cleancache();
      CACHEmiss++;
    }
    if ((gh = OV3opengroupfiles(group)) == NULL)
	return NULL;
    gh->hash = hash;
    gh->gloc = gloc;
    if (OV3mode & OV_WRITE) {
	/*
	 * Don't try to update ge->... unless we are in write mode, since
	 * otherwise we can't write to that mapped region. 
	 */
	ge->indexinode = gh->indexinode;
    }
    if (needcache) {
      ce = NEW(CACHEENTRY, 1);
      memset(ce, '\0', sizeof(*ce));
      ce->gh = gh;
      ce->refcount++;
      ce->lastused = Now;
    
      /* Insert into the list */
      ce->next = CACHEdata[hashbucket];
      CACHEdata[hashbucket] = ce;
      CACHEentries++;
    }
    return gh;
}

STATIC BOOL OV3closegroup(GROUPHANDLE *gh, BOOL needcache) {
    unsigned int        i;
    CACHEENTRY          *ce;

    if (needcache) {
      memcpy(&i, &gh->hash, sizeof(i));
      i %= CACHETABLESIZE;
      for (ce = CACHEdata[i]; ce != NULL; ce = ce->next) {
	if (memcmp(&ce->gh->hash, &gh->hash, sizeof(HASH)) == 0) {
	    ce->refcount--;
	    break;
	}
      }
    } else
      OV3closegroupfiles(gh);
    return TRUE;
}

STATIC BOOL OV3addrec(GROUPENTRY *ge, GROUPHANDLE *gh, int artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
    INDEXENTRY          ie;
    int                 base;
    
    if (ge->base == 0) {
	base = artnum > OV3padamount ? artnum - OV3padamount : 1;
    } else {
	base = ge->base;
	if (ge->base > artnum) {
	    syslog(L_ERROR, "tradindexed: could not add %s:%d, base == %d", gh->group, artnum, ge->base);
	    return FALSE;
	}
    }
    memset(&ie, '\0', sizeof(ie));
    if (write(gh->datafd, data, len) != len) {
	syslog(L_ERROR, "tradindexed: could not append overview record to %s: %m", gh->group);
	return FALSE;
    }
    if ((ie.offset = lseek(gh->datafd, 0, SEEK_CUR)) < 0) {
	syslog(L_ERROR, "tradindexed: could not get offset of overview record in %s: %m", gh->group);
	return FALSE;
    }
    ie.length = len;
    ie.offset -= ie.length;
    ie.arrived = arrived;
    ie.expires = expires;
    ie.token = token;
    
    if (pwrite(gh->indexfd, &ie, sizeof(ie), (artnum - base) * sizeof(ie)) != sizeof(ie)) {
	syslog(L_ERROR, "tradindexed: could not write index record for %s:%d", gh->group, artnum);
	return FALSE;
    }
    if ((ge->low <= 0) || (ge->low > artnum))
	ge->low = artnum;
    if ((ge->high <= 0) || (ge->high < artnum))
	ge->high = artnum;
    ge->count++;
    ge->base = base;
    return TRUE;
}

BOOL tradindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
    GROUPHANDLE         *gh;
    GROUPENTRY		*ge;
    GROUPLOC            gloc;

    if ((gh = OV3opengroup(group, TRUE)) == NULL) {
	/*
	** check to see if group is in group.index, and if not, just 
	** continue (presumably the group was rmgrouped earlier, but we
	** still have articles referring to it in spool).
	*/
	gloc = GROUPfind(group);
	if (GROUPLOCempty(gloc))
	    return TRUE;
	/*
	** group was there, but open failed for some reason, return err.
	*/
	return FALSE;
    }	

    /* pack group if needed. */
    ge = &GROUPentries[gh->gloc.recno];
    if (Cutofflow && ge->low > artnum) {
	OV3closegroup(gh, TRUE);
	return TRUE;
    }
    if (ge->base > artnum) {
	if (!OV3packgroup(group, OV3padamount + ge->base - artnum)) {
	    OV3closegroup(gh, TRUE);
	    return FALSE;
	}
	/* sigh. need to close and reopen group after packing. */
	OV3closegroup(gh, TRUE);
	if ((gh = OV3opengroup(group, TRUE)) == NULL)
	    return FALSE;
    }
    GROUPlock(gh->gloc, LOCK_WRITE);
    OV3addrec(ge, gh, artnum, token, data, len, arrived, expires);
    GROUPlock(gh->gloc, LOCK_UNLOCK);
    OV3closegroup(gh, TRUE);

    return TRUE;
}

BOOL tradindexed_cancel(TOKEN token) {
    return TRUE;
}

void *tradindexed_opensearch(char *group, int low, int high) {
    GROUPHANDLE         *gh;
    GROUPENTRY          *ge;
    ino_t               oldinode;
    int                 base;
    OV3SEARCH           *search;
    
    if ((gh = OV3opengroup(group, FALSE)) == NULL)
	return NULL;

    ge = &GROUPentries[gh->gloc.recno];
    /* It's possible that you could get two different files with the
       same inode here, but it's incredibly unlikely considering the
       run time of this loop */
    do {
	oldinode = ge->indexinode;
	if (low < ge->low)
	    low = ge->low;
	if (high > ge->high)
	    high = ge->high;
	base = ge->base;
    } while (ge->indexinode != oldinode);

    if (high < base || low < base) { /* return NULL if searching range is out */
      OV3closegroup(gh, FALSE);
      return NULL;
    }

    if (!OV3mmapgroup(gh)) {
	OV3closegroup(gh, FALSE);
	return NULL;
    }
    
    search = NEW(OV3SEARCH, 1);
    search->limit = high - base;
    search->cur = low - base;
    search->base = base;
    search->gh = gh;
    search->group = COPY(group);
    return (void *)search;
}

BOOL ov3search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived, time_t *expires) {
    OV3SEARCH           *search = (OV3SEARCH *)handle;
    INDEXENTRY           *ie;

    if (search->gh->datamem == (char *)-1 || search->gh->indexmem == (INDEXENTRY *)-1)
	return FALSE;
    for (ie = search->gh->indexmem;
	 ((char *)&ie[search->cur] < (char *)search->gh->indexmem + search->gh->indexlen) &&
	     (search->cur <= search->limit) &&
	     (ie[search->cur].length == 0);
	 search->cur++);

    if (search->cur > search->limit)
	return FALSE;

    if ((char *)&ie[search->cur] >= (char *)search->gh->indexmem + search->gh->indexlen) {
	/* don't claim, since highest article may be canceled and there may be
	   no index room for it */
	return FALSE;
    }

    ie = &ie[search->cur];
    if (ie->offset > search->gh->datalen || ie->offset + ie->length > search->gh->datalen)
	/* index may be corrupted, do not go further */
	return FALSE;

    if (artnum)
	*artnum = search->base + search->cur;
    if (len)
	*len = ie->length;
    if (data)
	*data = search->gh->datamem + ie->offset;
    if (token)
	*token = ie->token;
    if (arrived)
	*arrived = ie->arrived;
    if (expires)
	*expires = ie->expires;

    search->cur++;

    return TRUE;
}

BOOL tradindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived) {
  return(ov3search(handle, artnum, data, len, token, arrived, NULL));
}

void tradindexed_closesearch(void *handle) {
    OV3SEARCH           *search = (OV3SEARCH *)handle;

    OV3closegroup(search->gh, FALSE);
    DISPOSE(search->group);
    DISPOSE(search);
}

BOOL tradindexed_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token) {
    void                *handle;
    BOOL                retval;
    if (!(handle = tradindexed_opensearch(group, artnum, artnum)))
	return FALSE;
    retval = tradindexed_search(handle, NULL, data, len, token, NULL);
    tradindexed_closesearch(handle);
    return retval;
}

STATIC void OV3getdirpath(char *group, char *path) {
    char                *sepgroup;
    char                *p;
    char                **groupparts = NULL;
    int                 i, j;
    
    sepgroup = COPY(group);
    for (p = sepgroup; *p != '\0'; p++)
	if (*p == '.')
	    *p = ' ';
    
    i = argify(sepgroup, &groupparts);
    DISPOSE(sepgroup);
    strcpy(path, innconf->pathoverview);
    strcat(path, "/");
    for (p = path + strlen(path), j = 0; j < i; j++) {
	*p++ = groupparts[j][0];
	*p++ = '/';
    }
    *p = '\0';
    freeargify(&groupparts);
}

STATIC void OV3getIDXfilename(char *group, char *path) {
    char                *p;

    OV3getdirpath(group, path);
    p = path + strlen(path);
    sprintf(p, "%s.IDX", group);
}

STATIC void OV3getDATfilename(char *group, char *path) {
    char                *p;

    OV3getdirpath(group, path);
    p = path + strlen(path);
    sprintf(p, "%s.DAT", group);
}

/*
 * Shift group index file so it has lower value of base.
 */

STATIC BOOL OV3packgroup(char *group, int delta) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    GROUPENTRY		*ge;
    GROUPLOC            gloc;
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    struct stat         sb;
    int			fd;
    int			numentries;
    GROUPHANDLE		*gh;
    OFFSET_T		nbytes;

    if (delta <= 0) return FALSE;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return FALSE;
    ge = &GROUPentries[gloc.recno];
    if (ge->count == 0)
	return TRUE;

    syslog(L_NOTICE, "tradindexed: repacking group %s, offset %d", group, delta);
    GROUPlock(gloc, LOCK_WRITE);

    if (delta > ge->base) delta = ge->base;

    strcpy(bakgroup, group);
    strcat(bakgroup, "-BAK");
    strcpy(newgroup, group);
    strcat(newgroup, "-NEW"); 
    OV3getIDXfilename(group, oldidx);
    OV3getIDXfilename(newgroup, newidx);
    OV3getIDXfilename(bakgroup, bakidx);

    unlink(newidx);

    /* open and mmap old group index */
    if ((gh = OV3opengroup(group, FALSE)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    if (!OV3mmapgroup(gh)) {
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }

    if ((fd = open(newidx, O_RDWR | O_CREAT, 0660)) < 0) {
	syslog(L_ERROR, "tradindexed: could not open %s: %m", newidx);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }

    /* stat old index file so we know its actual size. */
    if (fstat(fd, &sb) < 0) {
	syslog(L_ERROR, "tradindexed: could not stat %s: %m", newidx);
	close(fd);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
	

    /* write old index records to new file */
    numentries = ge->high - ge->low + 1;
    nbytes = numentries * sizeof(INDEXENTRY);

    /*
    ** check to see if the actual file length is less than nbytes (since the 
    ** article numbers may be sparse) and if so, only read/write that amount. 
    */
    if (nbytes > sb.st_size) {
	nbytes = sb.st_size;
    }

    if (pwrite(fd, &gh->indexmem[ge->low - ge->base] , nbytes,
	       sizeof(INDEXENTRY)*(ge->low - ge->base + delta)) != nbytes) {
	syslog(L_ERROR, "tradindexed: packgroup cant write to %s: %m", newidx);
	close(fd);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }	
    if (close(fd) < 0) {
	syslog(L_ERROR, "tradindexed: packgroup cant close %s: %m", newidx);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", newidx, oldidx);
	    break;
	}
	unlink(bakidx);
    } while (0);
    ge->indexinode = sb.st_ino;
    ge->base -= delta;
    GROUPlock(gloc, LOCK_UNLOCK);
    OV3closegroup(gh, FALSE);
    return TRUE;
}

BOOL tradindexed_expiregroup(char *group, int *lo) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    void                *handle;
    GROUPENTRY          *ge;
    GROUPLOC            gloc;
    char                *data;
    int                 len;
    TOKEN               token;
    ARTNUM              artnum;
    GROUPENTRY          newge;
    GROUPHANDLE         *newgh;
    ARTHANDLE           *ah;
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    char                bakdat[BIG_BUFFER], olddat[BIG_BUFFER], newdat[BIG_BUFFER];
    struct stat         sb;
    time_t		arrived, expires;

    if (group == NULL)
	return TRUE;
    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return FALSE;
    ge = &GROUPentries[gloc.recno];
    if (ge->count == 0) {
	if (lo)
	    *lo = ge->low;
	return TRUE;
    }
    
    strcpy(bakgroup, group);
    strcat(bakgroup, "-BAK");
    strcpy(newgroup, group);
    strcat(newgroup, "-NEW"); 
    OV3getIDXfilename(group, oldidx);
    OV3getIDXfilename(newgroup, newidx);
    OV3getIDXfilename(bakgroup, bakidx);
    OV3getDATfilename(group, olddat);
    OV3getDATfilename(newgroup, newdat);
    OV3getDATfilename(bakgroup, bakdat);

    unlink(newidx);
    unlink(newdat);

    GROUPlock(gloc, LOCK_WRITE);
    if ((handle = tradindexed_opensearch(group, ge->low, ge->high)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    if ((newgh = OV3opengroupfiles(newgroup)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	tradindexed_closesearch(handle);
	return FALSE;
    }
    newge = *ge;
    newge.base = newge.low = newge.count = 0;
    while (ov3search(handle, &artnum, &data, &len, &token, &arrived, &expires)) {
	ah = NULL;
	if (SMprobe(SELFEXPIRE, &token, NULL)) {
	    if ((ah = SMretrieve(token, RETR_STAT)) == NULL)
		continue;
	} else {
	    if (!innconf->groupbaseexpiry && !OVhisthasmsgid(data))
		continue; 
	}
	if (ah)
	    SMfreearticle(ah);
	if (innconf->groupbaseexpiry && OVgroupbasedexpire(token, group, data, len, arrived, expires))
	    continue;
#if 0
	if (p = strchr(data, '\t')) {
	    for (p--; (p >= data) && isdigit((int) *p); p--);
	    if (p >= data) {
		printf("bad article %s:%d\n", group, artnum);
		sprintf(overdata, "%d\t", artnum);
		i = strlen(overdata);
		p = overdata + i;
		memcpy(p, data, len - newlen - 2);
		p = overdata + len - 2;
		memcpy(p, "\r\n", 2);
		OV3addrec(&newge, newgh, artnum, token, overdata, len, arrived, expires);
		continue;
	    }
	}
	if (atoi(data) != artnum) {
	    printf("misnumbered article %s:%d\n", group, artnum);
	    if ((p = strstr(data, "Xref: ")) == NULL) {
		syslog(L_ERROR, "tradindexed: could not find Xref header in %s:%d", group, artnum);
		continue;
	    }
	    if ((p = strchr(p, ' ')) == NULL) {
		syslog(L_ERROR, "tradindexed: could not find space after Xref header in %s:%d", group, artnum);
		continue;
	    }
	    if ((p = strstr(p, group)) == NULL) {
		syslog(L_ERROR, "tradindexed: could not find group name in Xref header in %s:%d", group, artnum);
		continue;
	    }
	    p += strlen(group) + 1;
	    artnum = atoi(p);
	}
#endif
	OV3addrec(&newge, newgh, artnum, token, data, len, arrived, expires);
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(olddat, bakdat) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", olddat, bakdat);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", newidx, oldidx);
	    break;
	}
	if (rename(newdat, olddat) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", newdat, olddat);
	    break;
	}
	unlink(bakidx);
	unlink(bakdat);
    } while (0);
    if (newge.low == 0)
	/* no article for the group */
	newge.low = newge.high;
    *ge = newge;
    ge->indexinode = sb.st_ino;
    if (lo) {
	if (ge->count == 0)
	    /* lomark should be himark + 1, if no article for the group */
	    *lo = ge->low + 1;
	else
	    *lo = ge->low;
    }
    GROUPlock(gloc, LOCK_UNLOCK);
    tradindexed_closesearch(handle);
    OV3closegroupfiles(newgh);
    return TRUE;
}

BOOL tradindexed_ctl(OVCTLTYPE type, void *val) {
    int *i;
    OVSORTTYPE *sorttype;
    switch (type) {
    case OVSPACE:
	i = (int *)val;
	*i = -1;
	return TRUE;
    case OVSORT:
	sorttype = (OVSORTTYPE *)val;
	*sorttype = OVNEWSGROUP;
	return TRUE;
      case OVCUTOFFLOW:
	Cutofflow = *(BOOL *)val;
	return TRUE;
    case OVSTATICSEARCH:
	i = (int *)val;
	*i = FALSE;
	return TRUE;
    default:
	return FALSE;
    }
}

void tradindexed_close(void) {
    int                 i;
    CACHEENTRY          *ce, *next;
    struct stat         sb;

    for (i = 0; i < CACHETABLESIZE; i++) {
	for (ce = CACHEdata[i]; ce != NULL; ce = ce->next) {
	    OV3closegroupfiles(ce->gh);
	}
	for (ce = CACHEdata[i]; ce != NULL; ce = next) {
	    next = ce->next;
	    ce->next = NULL;
	    DISPOSE(ce);
	}
    }
    if (fstat(GROUPfd, &sb) < 0)
	return;
    close(GROUPfd);

    if (GROUPheader) {
	if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
	    syslog(L_FATAL, "tradindexed: could not munmap group.index in tradindexed_close: %m");
	    return;
	}
    }
}
