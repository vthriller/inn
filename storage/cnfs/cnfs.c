/*  $Id$
**
**  cyclic news file system
*/

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <limits.h>
#include <syslog.h> 
#include <macros.h>
#include <configdata.h>
#include <clibrary.h>
#include <libinn.h>
#include <methods.h>

#include <interface.h>
#include "cnfs.h"
#include "cnfs-private.h"
#include "paths.h"

typedef struct {
    /**** Stuff to be cleaned up when we're done with the article */
    char		*base;		/* Base of mmap()ed art */
    int			len;		/* Length of article (and thus
					   mmap()ed art */
    CYCBUFF		*cycbuff;	/* pointer to current CYCBUFF */
    CYCBUFF_OFF_T	offset;		/* offset to current article */
    BOOL		rollover;	/* true if the search is rollovered */
} PRIV_CNFS;

STATIC char LocalLogName[] = "CNFS-sm";
STATIC CYCBUFF		*cycbufftab = (CYCBUFF *)NULL;
STATIC METACYCBUFF 	*metacycbufftab = (METACYCBUFF *)NULL;
STATIC CNFSEXPIRERULES	*metaexprulestab = (CNFSEXPIRERULES *)NULL;
STATIC long		pagesize = 0;
STATIC int		metabuff_update = METACYCBUFF_UPDATE;
STATIC int		refresh_interval = REFRESH_INTERVAL;

STATIC TOKEN CNFSMakeToken(char *cycbuffname, CYCBUFF_OFF_T offset,
		       U_INT32_T cycnum, STORAGECLASS class, TOKEN *oldtoken) {
    TOKEN               token;
    INT32_T		int32;

    if (oldtoken == (TOKEN *)NULL)
	memset(&token, '\0', sizeof(token));
    else
	memcpy(&token, oldtoken, sizeof(token));
    /*
    ** XXX We'll assume that TOKENSIZE is 16 bytes and that we divvy it
    ** up as: 8 bytes for cycbuffname, 4 bytes for offset, 4 bytes
    ** for cycnum.  See also: CNFSBreakToken() for hard-coded constants.
    */
    token.type = TOKEN_CNFS;
    token.class = class;
    memcpy(token.token, cycbuffname, CNFSMAXCYCBUFFNAME);
    int32 = htonl(offset / CNFS_BLOCKSIZE);
    memcpy(&token.token[8], &int32, sizeof(int32));
    int32 = htonl(cycnum);
    memcpy(&token.token[12], &int32, sizeof(int32));
    return token;
}

/*
** NOTE: We assume that cycbuffname is 9 bytes long.
*/

STATIC BOOL CNFSBreakToken(TOKEN token, char *cycbuffname,
			   CYCBUFF_OFF_T *offset, U_INT32_T *cycnum) {
    INT32_T	int32;

    if (cycbuffname == NULL || offset == NULL || cycnum == NULL) {
	syslog(L_ERROR, "%s: BreakToken: invalid argument",
	       LocalLogName, cycbuffname);
	SMseterror(SMERR_INTERNAL, "BreakToken: invalid argument");
	return FALSE;
    }
    memcpy(cycbuffname, token.token, CNFSMAXCYCBUFFNAME);
    *(cycbuffname + CNFSMAXCYCBUFFNAME) = '\0';	/* Just to be paranoid */
    memcpy(&int32, &token.token[8], sizeof(int32));
    *offset = (CYCBUFF_OFF_T)ntohl(int32) * (CYCBUFF_OFF_T)CNFS_BLOCKSIZE;
    memcpy(&int32, &token.token[12], sizeof(int32));
    *cycnum = ntohl(int32);
    return TRUE;
}

STATIC char hextbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'a', 'b', 'c', 'd', 'e', 'f'};

/*
** CNFSofft2hex -- Given an argument of type CYCBUFF_OFF_T, return
**	a static ASCII string representing its value in hexadecimal.
**
**	If "leadingzeros" is true, the number returned will have leading 0's.
*/

STATIC char * CNFSofft2hex(CYCBUFF_OFF_T offset, BOOL leadingzeros) {
    static char	buf[24];
    char	*p;

    if (sizeof(CYCBUFF_OFF_T) <= 4) {
	sprintf(buf, (leadingzeros) ? "%016lx" : "%lx", offset);
    } else { 
	int	i;

	for (i = 0; i < CNFSLASIZ; i++)
	    buf[i] = '0';	/* Pad with zeros to start */
	for (i = CNFSLASIZ - 1; i >= 0; i--) {
	    buf[i] = hextbl[offset & 0xf];
	    offset >>= 4;
	}
    }
    if (! leadingzeros) {
	for (p = buf; *p == '0'; p++)
	    ;
	if (*p != '\0')
		return p;
	else
		return p - 1;	/* We converted a "0" and then bypassed all
				   the zeros */
    } else 
	return buf;
}

/*
** CNFShex2offt -- Given an ASCII string containing a hexadecimal representation
**	of a CYCBUFF_OFF_T, return a CYCBUFF_OFF_T.
*/

STATIC CYCBUFF_OFF_T CNFShex2offt(char *hex) {
    if (sizeof(CYCBUFF_OFF_T) <= 4) {
	CYCBUFF_OFF_T	rpofft;
	/* I'm lazy */
	sscanf(hex, "%lx", &rpofft);
	return rpofft;
    } else {
	char		diff;
	CYCBUFF_OFF_T	n = (CYCBUFF_OFF_T) 0;

	for (; *hex != '\0'; hex++) {
	    if (*hex >= '0' && *hex <= '9')
		diff = '0';
	    else if (*hex >= 'a' && *hex <= 'f')
		diff = 'a' - 10;
	    else if (*hex >= 'A' && *hex <= 'F')
		diff = 'A' - 10;
	    else {
		/*
		** We used to have a syslog() message here, but the case
		** where we land here because of a ":" happens, er, often.
		*/
		break;
	    }
	    n += (*hex - diff);
	    if (isalnum((int)*(hex + 1)))
		n <<= 4;
	}
	return n;
    }
}

STATIC void CNFScleancycbuff(void) {
    CYCBUFF	*cycbuff, *nextcycbuff;

    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL;) {
      if (cycbuff->fd >= 0)
	close(cycbuff->fd);
      cycbuff->fd = -1;
      nextcycbuff = cycbuff->next;
      DISPOSE(cycbuff);
      cycbuff = nextcycbuff;
    }
    cycbufftab = (CYCBUFF *)NULL;
}

STATIC void CNFScleanmetacycbuff(void) {
    METACYCBUFF	*metacycbuff, *nextmetacycbuff;

    for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *)NULL;) {
      nextmetacycbuff = metacycbuff->next;
      DISPOSE(metacycbuff->members);
      DISPOSE(metacycbuff->name);
      DISPOSE(metacycbuff);
      metacycbuff = nextmetacycbuff;
    }
    metacycbufftab = (METACYCBUFF *)NULL;
}

STATIC void CNFScleanexpirerule(void) {
    CNFSEXPIRERULES	*metaexprule, *nextmetaexprule;

    for (metaexprule = metaexprulestab; metaexprule != (CNFSEXPIRERULES *)NULL;) {
      nextmetaexprule = metaexprule->next;
      DISPOSE(metaexprule);
      metaexprule = nextmetaexprule;
    }
    metaexprulestab = (CNFSEXPIRERULES *)NULL;
}

STATIC CYCBUFF *CNFSgetcycbuffbyname(char *name) {
    CYCBUFF	*cycbuff;
 
    if (name == NULL)
	return NULL;
    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next)
	if (strcmp(name, cycbuff->name) == 0) 
	    return cycbuff;
    return NULL;
}

STATIC METACYCBUFF *CNFSgetmetacycbuffbyname(char *name) {
  METACYCBUFF	*metacycbuff;

  if (name == NULL)
    return NULL;
  for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *)NULL; metacycbuff = metacycbuff->next)
    if (strcmp(name, metacycbuff->name) == 0) 
      return metacycbuff;
  return NULL;
}

STATIC BOOL CNFSflushhead(CYCBUFF *cycbuff) {
  int			b;
  CYCBUFFEXTERN		rpx;

  if (!cycbuff->needflush)
    return TRUE;
  memset(&rpx, 0, sizeof(CYCBUFFEXTERN));
  if (CNFSseek(cycbuff->fd, (CYCBUFF_OFF_T) 0, SEEK_SET) < 0) {
    syslog(L_ERROR, "CNFSflushhead: magic CNFSseek failed: %m");
    return FALSE;
  }
  if (cycbuff->magicver == 3) {
    cycbuff->updated = time(NULL);
    strncpy(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3));
    strncpy(rpx.name, cycbuff->name, CNFSNASIZ);
    strncpy(rpx.path, cycbuff->path, CNFSPASIZ);
    /* Don't use sprintf() directly ... the terminating '\0' causes grief */
    strncpy(rpx.lena, CNFSofft2hex(cycbuff->len, TRUE), CNFSLASIZ);
    strncpy(rpx.freea, CNFSofft2hex(cycbuff->free, TRUE), CNFSLASIZ);
    strncpy(rpx.cyclenuma, CNFSofft2hex(cycbuff->cyclenum, TRUE), CNFSLASIZ);
    strncpy(rpx.updateda, CNFSofft2hex(cycbuff->updated, TRUE), CNFSLASIZ);
    strncpy(rpx.metaname, cycbuff->metaname, CNFSNASIZ);
    strncpy(rpx.orderinmeta, CNFSofft2hex(cycbuff->order, TRUE), CNFSLASIZ);
    if (cycbuff->currentbuff) {
	strncpy(rpx.currentbuff, "TRUE", CNFSMASIZ);
    } else {
	strncpy(rpx.currentbuff, "FALSE", CNFSMASIZ);
    }
    if ((b = write(cycbuff->fd, &rpx, sizeof(CYCBUFFEXTERN))) != sizeof(CYCBUFFEXTERN)) {
      syslog(L_ERROR, "%s: CNFSflushhead: write failed (%d bytes): %m", LocalLogName, b);
      return FALSE;
    }
    cycbuff->needflush = FALSE;
  } else {
    syslog(L_ERROR, "%s: CNFSflushhead: bogus magicver for %s: %d",
      LocalLogName, cycbuff->name, cycbuff->magicver);
    return FALSE;
  }
  return TRUE;
}

STATIC void CNFSflushallheads(void) {
  CYCBUFF	*cycbuff;

  for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
    if (cycbuff->needflush)
	syslog(L_NOTICE, "%s: CNFSflushallheads: flushing %s", LocalLogName, cycbuff->name);
    (void)CNFSflushhead(cycbuff);
  }
}

/*
** CNFSReadFreeAndCycle() -- Read from disk the current values of CYCBUFF's
**	free pointer and cycle number.  Return 1 on success, 0 otherwise.
*/

STATIC void CNFSReadFreeAndCycle(CYCBUFF *cycbuff) {
    CYCBUFFEXTERN	rpx;
    char		buf[64];

    if (CNFSseek(cycbuff->fd, (CYCBUFF_OFF_T) 0, SEEK_SET) < 0) {
	syslog(L_ERROR, "CNFSReadFreeAndCycle: magic lseek failed: %m");
	SMseterror(SMERR_UNDEFINED, NULL);
	return;
    }
    if (read(cycbuff->fd, &rpx, sizeof(CYCBUFFEXTERN)) != sizeof(rpx)) {
	syslog(L_ERROR, "CNFSReadFreeAndCycle: magic read failed: %m");
	SMseterror(SMERR_UNDEFINED, NULL);
	return;
    }
    /* Sanity checks are not needed since CNFSinit_disks() has already done. */
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.freea, CNFSLASIZ);
    cycbuff->free = CNFShex2offt(buf);
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.updateda, CNFSLASIZ);
    cycbuff->updated = CNFShex2offt(buf);
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
    cycbuff->cyclenum = CNFShex2offt(buf);
    return;
}

STATIC BOOL CNFSparse_part_line(char *l) {
  char		*p;
  struct stat	sb;
  CYCBUFF_OFF_T	len, minartoffset;
  int		tonextblock;
  CYCBUFF	*cycbuff, *tmp;

  /* Symbolic cnfs partition name */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > CNFSMAXCYCBUFFNAME - 1) {
    syslog(L_ERROR, "%s: bad cycbuff name in line '%s'", LocalLogName, l);
    return FALSE;
  }
  *p = '\0';
  cycbuff = NEW(CYCBUFF, 1);
  memset(cycbuff->name, '\0', CNFSNASIZ);
  strcpy(cycbuff->name, l);
  l = ++p;

  /* Path to cnfs partition */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > CNFSPASIZ - 1) {
    syslog(L_ERROR, "%s: bad pathname in line '%s'", LocalLogName, l);
    DISPOSE(cycbuff);
    return FALSE;
  }
  *p = '\0';
  memset(cycbuff->path, '\0', CNFSPASIZ);
  strcpy(cycbuff->path, l);
  if (stat(cycbuff->path, &sb) < 0) {
    syslog(L_ERROR, "%s: file '%s' does not exist, ignoring '%s' cycbuff",
	   LocalLogName, cycbuff->path, cycbuff->name);
    DISPOSE(cycbuff);
    return FALSE;
  }
  l = ++p;

  /* Length/size of symbolic partition */
  len = strtoul(l, NULL, 10) * (CYCBUFF_OFF_T)1024;	/* This value in KB in decimal */
  if (S_ISREG(sb.st_mode) && len != sb.st_size) {
    if (sizeof(CYCBUFFEXTERN) > sb.st_size) {
      syslog(L_NOTICE, "%s: length must be at least '%ld' for '%s' cycbuff(%ld bytes)",
	LocalLogName, sizeof(CYCBUFFEXTERN), cycbuff->name, sb.st_size);
      DISPOSE(cycbuff);
      return FALSE;
    }
  }
  cycbuff->len = len;
  cycbuff->fd = -1;
  cycbuff->next = (CYCBUFF *)NULL;
  cycbuff->needflush = FALSE;
  cycbuff->bitfield = (caddr_t)NULL;
  /*
  ** The minimum article offset will be the size of the bitfield itself,
  ** len / (blocksize * 8), plus however many additional blocks the CYCBUFF
  ** external header occupies ... then round up to the next block.
  */
  minartoffset =
      cycbuff->len / (CNFS_BLOCKSIZE * 8) + CNFS_BEFOREBITF;
  tonextblock = CNFS_HDR_PAGESIZE - (minartoffset & (CNFS_HDR_PAGESIZE - 1));
  cycbuff->minartoffset = minartoffset + tonextblock;

  if (cycbufftab == (CYCBUFF *)NULL)
    cycbufftab = cycbuff;
  else {
    for (tmp = cycbufftab; tmp->next != (CYCBUFF *)NULL; tmp = tmp->next);
    tmp->next = cycbuff;
  }
  /* Done! */
  return TRUE;
}

STATIC BOOL CNFSparse_metapart_line(char *l) {
  char		*p, *cycbuff, *q = l;
  CYCBUFF	*rp;
  METACYCBUFF	*metacycbuff, *tmp;

  /* Symbolic metacycbuff name */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0) {
    syslog(L_ERROR, "%s: bad partition name in line '%s'", LocalLogName, l);
    return FALSE;
  }
  *p = '\0';
  metacycbuff = NEW(METACYCBUFF, 1);
  metacycbuff->members = (CYCBUFF **)NULL;
  metacycbuff->count = 0;
  metacycbuff->name = COPY(l);
  metacycbuff->next = (METACYCBUFF *)NULL;
  metacycbuff->metamode = INTERLEAVE;
  l = ++p;

  if ((p = strchr(l, ':')) != NULL) {
      if (p - l <= 0) {
	syslog(L_ERROR, "%s: bad mode in line '%s'", LocalLogName, q);
	return FALSE;
      }
      if (strcmp(++p, "INTERLEAVE") == 0)
	metacycbuff->metamode = INTERLEAVE;
      else if (strcmp(p, "SEQUENTIAL") == 0)
	metacycbuff->metamode = SEQUENTIAL;
      else {
	syslog(L_ERROR, "%s: unknown mode in line '%s'", LocalLogName, q);
	return FALSE;
      }
      *--p = '\0';
  }
  /* Cycbuff list */
  while ((p = strchr(l, ',')) != NULL && p - l > 0) {
    *p = '\0';
    cycbuff = l;
    l = ++p;
    if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
      syslog(L_ERROR, "%s: bogus cycbuff '%s' (metacycbuff '%s')",
	     LocalLogName, cycbuff, metacycbuff->name);
      DISPOSE(metacycbuff->members);
      DISPOSE(metacycbuff->name);
      DISPOSE(metacycbuff);
      return FALSE;
    }
    if (metacycbuff->count == 0)
      metacycbuff->members = NEW(CYCBUFF *, 1);
    else 
      RENEW(metacycbuff->members, CYCBUFF *, metacycbuff->count + 1);
    metacycbuff->members[metacycbuff->count++] = rp;
  }
  /* Gotta deal with the last cycbuff on the list */
  cycbuff = l;
  if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
    syslog(L_ERROR, "%s: bogus cycbuff '%s' (metacycbuff '%s')",
	   LocalLogName, cycbuff, metacycbuff->name);
    DISPOSE(metacycbuff->members);
    DISPOSE(metacycbuff->name);
    DISPOSE(metacycbuff);
    return FALSE;
  } else {
    if (metacycbuff->count == 0)
      metacycbuff->members = NEW(CYCBUFF *, 1);
    else 
      RENEW(metacycbuff->members, CYCBUFF *, metacycbuff->count + 1);
    metacycbuff->members[metacycbuff->count++] = rp;
  }
  
  if (metacycbuff->count == 0) {
    syslog(L_ERROR, "%s: no cycbuffs assigned to cycbuff '%s'",
	   LocalLogName, metacycbuff->name);
    DISPOSE(metacycbuff->name);
    DISPOSE(metacycbuff);
    return FALSE;
  }
  if (metacycbufftab == (METACYCBUFF *)NULL)
    metacycbufftab = metacycbuff;
  else {
    for (tmp = metacycbufftab; tmp->next != (METACYCBUFF *)NULL; tmp = tmp->next);
    tmp->next = metacycbuff;
  }
  /* DONE! */
  return TRUE;
}

STATIC BOOL CNFSparse_groups_line() {
  METACYCBUFF	*mrp;
  STORAGE_SUB	*sub = (STORAGE_SUB *)NULL;
  CNFSEXPIRERULES	*metaexprule, *tmp;

  sub = SMGetConfig(TOKEN_CNFS, sub);
  for (;sub != (STORAGE_SUB *)NULL; sub = SMGetConfig(TOKEN_CNFS, sub)) {
    if (sub->options == (char *)NULL) {
      syslog(L_ERROR, "%s: storage.conf options field is missing",
	   LocalLogName);
      CNFScleanexpirerule();
      return FALSE;
    }
    if ((mrp = CNFSgetmetacycbuffbyname(sub->options)) == NULL) {
      syslog(L_ERROR, "%s: storage.conf options field '%s' undefined",
	   LocalLogName, sub->options);
      CNFScleanexpirerule();
      return FALSE;
    }
    metaexprule = NEW(CNFSEXPIRERULES, 1);
    metaexprule->class = sub->class;
    metaexprule->dest = mrp;
    metaexprule->next = (CNFSEXPIRERULES *)NULL;
    if (metaexprulestab == (CNFSEXPIRERULES *)NULL)
      metaexprulestab = metaexprule;
    else {
      for (tmp = metaexprulestab; tmp->next != (CNFSEXPIRERULES *)NULL; tmp = tmp->next);
      tmp->next = metaexprule;
    }
  }
  /* DONE! */
  return TRUE;
}

/*
** CNFSinit_disks -- Finish initializing cycbufftab
**	Called by "innd" only -- we open (and keep) a read/write
**	file descriptor for each CYCBUFF.
**
** Calling this function repeatedly shouldn't cause any harm
** speed-wise or bug-wise, as long as the caller is accessing the
** CYCBUFFs _read-only_.  If innd calls this function repeatedly,
** bad things will happen.
*/

STATIC BOOL CNFSinit_disks(CYCBUFF *cycbuff) {
  char		buf[64];
  CYCBUFFEXTERN	rpx;
  int		fd, bytes;
  CYCBUFF_OFF_T	tmpo;
  BOOL		oneshot;

  /*
  ** Discover the state of our cycbuffs.  If any of them are in icky shape,
  ** duck shamelessly & return FALSE.
  */

  if (cycbuff != (CYCBUFF *)NULL)
    oneshot = TRUE;
  else {
    oneshot = FALSE;
    cycbuff = cycbufftab;
  }
  for (; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
    if (strcmp(cycbuff->path, "/dev/null") == 0) {
	syslog(L_ERROR, "%s: ERROR opening '%s' is not available",
		LocalLogName, cycbuff->path);
	return FALSE;
    }
    if (cycbuff->fd < 0) {
	if ((fd = open(cycbuff->path, SMopenmode ? O_RDWR : O_RDONLY)) < 0) {
	    syslog(L_ERROR, "%s: ERROR opening '%s' O_RDONLY : %m",
		   LocalLogName, cycbuff->path);
	    return FALSE;
	} else {
	    CloseOnExec(fd, 1);
	    cycbuff->fd = fd;
	}
    }
    if ((tmpo = CNFSseek(cycbuff->fd, (CYCBUFF_OFF_T) 0, SEEK_SET)) < 0) {
	syslog(L_ERROR, "%s: pre-magic read lseek failed: %m", LocalLogName);
	return FALSE;
    }
    if ((bytes = read(cycbuff->fd, &rpx, sizeof(CYCBUFFEXTERN))) != sizeof(rpx)) {
	syslog(L_ERROR, "%s: read magic failed %d bytes: %m", LocalLogName, bytes);
	return FALSE;
    }
    if (CNFSseek(cycbuff->fd, tmpo, SEEK_SET) != tmpo) {
	syslog(L_ERROR, "%s: post-magic read lseek to 0x%s failed: %m",
	       LocalLogName, CNFSofft2hex(tmpo, FALSE));
	return FALSE;
    }

    /*
    ** Much of this checking from previous revisions is (probably) bogus
    ** & buggy & particularly icky & unupdated.  Use at your own risk.  :-)
    */

    if (strncmp(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3)) == 0) {
	cycbuff->magicver = 3;
	if (strncmp(rpx.name, cycbuff->name, CNFSNASIZ) != 0) {
	    syslog(L_ERROR, "%s: Mismatch 3: read %s for cycbuff %s", LocalLogName,
		   rpx.name, cycbuff->name);
	    return FALSE;
	}
	if (strncmp(rpx.path, cycbuff->path, CNFSPASIZ) != 0) {
	    syslog(L_ERROR, "%s: Path mismatch: read %s for cycbuff %s",
		   LocalLogName, rpx.path, cycbuff->path);
	} 
	strncpy(buf, rpx.lena, CNFSLASIZ);
	buf[CNFSLASIZ] = '\0';
	tmpo = CNFShex2offt(buf);
	if (tmpo != cycbuff->len) {
	    syslog(L_ERROR, "%s: Mismatch: read 0x%s length for cycbuff %s",
		   LocalLogName, CNFSofft2hex(tmpo, FALSE), cycbuff->path);
	    return FALSE;
	}
	buf[CNFSLASIZ] = '\0';
	strncpy(buf, rpx.freea, CNFSLASIZ);
	cycbuff->free = CNFShex2offt(buf);
	buf[CNFSLASIZ] = '\0';
	strncpy(buf, rpx.updateda, CNFSLASIZ);
	cycbuff->updated = CNFShex2offt(buf);
	buf[CNFSLASIZ] = '\0';
	strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
	cycbuff->cyclenum = CNFShex2offt(buf);
	strncpy(cycbuff->metaname, rpx.metaname, CNFSLASIZ);
	strncpy(buf, rpx.orderinmeta, CNFSLASIZ);
	cycbuff->order = CNFShex2offt(buf);
	if (strncmp(rpx.currentbuff, "TRUE", CNFSMASIZ) == 0) {
	    cycbuff->currentbuff = TRUE;
	} else
	    cycbuff->currentbuff = FALSE;
    } else {
	syslog(L_NOTICE,
		"%s: No magic cookie found for cycbuff %s, initializing",
		LocalLogName, cycbuff->name);
	cycbuff->magicver = 3;
	cycbuff->free = cycbuff->minartoffset;
	cycbuff->updated = 0;
	cycbuff->cyclenum = 1;
	cycbuff->currentbuff = TRUE;
	cycbuff->order = 0;	/* to indicate this is newly added cycbuff */
	cycbuff->needflush = TRUE;
	if (!CNFSflushhead(cycbuff))
	    return FALSE;
    }
    errno = 0;
    fd = cycbuff->fd;
    if ((cycbuff->bitfield =
	 mmap((caddr_t) 0, cycbuff->minartoffset, SMopenmode ? (PROT_READ | PROT_WRITE) : PROT_READ,
	      MAP_SHARED, fd, (off_t) 0)) == (MMAP_PTR) -1 || errno != 0) {
	syslog(L_ERROR,
	       "%s: CNFSinitdisks: mmap for %s offset %d len %d failed: %m",
	       LocalLogName, cycbuff->path, 0, cycbuff->minartoffset);
	return FALSE;
    }
    if (oneshot)
      break;
  }
  return TRUE;
}

STATIC BOOL CNFS_setcurrent(METACYCBUFF *metacycbuff) {
  CYCBUFF	*cycbuff;
  int		i, currentcycbuff, order = -1;
  BOOL		foundcurrent = FALSE;
  for (i = 0 ; i < metacycbuff->count ; i++) {
    cycbuff = metacycbuff->members[i];
    if (strncmp(cycbuff->metaname, metacycbuff->name, CNFSNASIZ) != 0) {
      /* this cycbuff is moved from other metacycbuff , or is new */
      cycbuff->order = i + 1;
      cycbuff->currentbuff = FALSE;
      strncpy(cycbuff->metaname, metacycbuff->name, CNFSLASIZ);
      cycbuff->needflush = TRUE;
      continue;
    }
    if (foundcurrent == FALSE && cycbuff->currentbuff == TRUE) {
      currentcycbuff = i;
      foundcurrent = TRUE;
    }
    if (foundcurrent == FALSE || order == -1 || order > cycbuff->order) {
      /* this cycbuff is a candidate for current cycbuff */
      currentcycbuff = i;
      order = cycbuff->order;
    }
    if (cycbuff->order != i + 1) {
      /* cycbuff order seems to be changed */
      cycbuff->order = i + 1;
      cycbuff->needflush = TRUE;
    }
  }
  /* If no current cycbuff found (say, all our cycbuffs are new) default to 0 */
  if (foundcurrent == FALSE) {
    currentcycbuff = 0;
  }
  for (i = 0 ; i < metacycbuff->count ; i++) {
    cycbuff = metacycbuff->members[i];
    if (currentcycbuff == i && cycbuff->currentbuff == FALSE) {
      cycbuff->currentbuff = TRUE;
      cycbuff->needflush = TRUE;
    }
    if (currentcycbuff != i && cycbuff->currentbuff == TRUE) {
      cycbuff->currentbuff = FALSE;
      cycbuff->needflush = TRUE;
    }
    if (cycbuff->needflush == TRUE && !CNFSflushhead(cycbuff))
	return FALSE;
  }
  metacycbuff->memb_next = currentcycbuff;
  return TRUE;
}

/*
** CNFSread_config() -- Read the cnfs partition/file configuration file.
**
** Oh, for the want of Perl!  My parser probably shows that I don't use
** C all that often anymore....
*/

STATIC BOOL CNFSread_config(void) {
    char	*config, *from, *to, **ctab = (char **)NULL;
    int		ctab_free = 0;	/* Index to next free slot in ctab */
    int		ctab_i;
    BOOL	metacycbufffound = FALSE;
    BOOL	cycbuffupdatefound = FALSE;
    BOOL	refreshintervalfound = FALSE;
    int		update, refresh;

    if ((config = ReadInFile(cpcatpath(innconf->pathetc, _PATH_CYCBUFFCONFIG),
				(struct stat *)NULL)) == NULL) {
	syslog(L_ERROR, "%s: cannot read %s", LocalLogName,
		cpcatpath(innconf->pathetc, _PATH_CYCBUFFCONFIG), NULL);
	DISPOSE(config);
	return FALSE;
    }
    for (from = to = config; *from; ) {
	if (ctab_free == 0)
	  ctab = NEW(char *, 1);
	else
	  RENEW(ctab, char *, ctab_free+1);
	if (*from == '#') {	/* Comment line? */
	    while (*from && *from != '\n')
		from++;				/* Skip past it */
	    from++;
	    continue;				/* Back to top of loop */
	}
	if (*from == '\n') {	/* End or just a blank line? */
	    from++;
	    continue;				/* Back to top of loop */
	}
	/* If we're here, we've got the beginning of a real entry */
	ctab[ctab_free++] = to = from;
	while (1) {
	    if (*from && *from == '\\' && *(from + 1) == '\n') {
		from += 2;		/* Skip past backslash+newline */
		while (*from && isspace((int)*from))
		    from++;
		continue;
	    }
	    if (*from && *from != '\n')
		*to++ = *from++;
	    if (*from == '\n') {
		*to++ = '\0';
		from++;
		break;
	    }
	    if (! *from)
		break;
	}
    }

    for (ctab_i = 0; ctab_i < ctab_free; ctab_i++) {
	if (strncmp(ctab[ctab_i], "cycbuff:", 8) == 0) {
	    if (metacycbufffound) {
		syslog(L_ERROR, "%s: all cycbuff entries shoud be before metacycbuff entries", LocalLogName);
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	    if (!CNFSparse_part_line(ctab[ctab_i] + 8)) {
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	} else if (strncmp(ctab[ctab_i], "metacycbuff:", 12) == 0) {
	    metacycbufffound = TRUE;
	    if (!CNFSparse_metapart_line(ctab[ctab_i] + 12)) {
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	} else if (strncmp(ctab[ctab_i], "cycbuffupdate:", 14) == 0) {
	    if (cycbuffupdatefound) {
		syslog(L_ERROR, "%s: duplicate cycbuffupdate entries", LocalLogName);
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	    cycbuffupdatefound = TRUE;
	    update = atoi(ctab[ctab_i] + 14);
	    if (update < 0) {
		syslog(L_ERROR, "%s: invalid cycbuffupdate", LocalLogName);
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	    if (update == 0)
		metabuff_update = METACYCBUFF_UPDATE;
	    else
		metabuff_update = update;
	} else if (strncmp(ctab[ctab_i], "refreshinterval:", 16) == 0) {
	    if (refreshintervalfound) {
		syslog(L_ERROR, "%s: duplicate refreshinterval entries", LocalLogName);
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	    refreshintervalfound = TRUE;
	    refresh = atoi(ctab[ctab_i] + 16);
	    if (refresh < 0) {
		syslog(L_ERROR, "%s: invalid refreshinterval", LocalLogName);
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	    if (refresh == 0)
		refresh_interval = REFRESH_INTERVAL;
	    else
		refresh_interval = refresh;
	} else {
	    syslog(L_ERROR, "%s: Bogus metacycbuff config line '%s' ignored",
		   LocalLogName, ctab[ctab_i]);
	}
    }
    DISPOSE(config);
    DISPOSE(ctab);
    if (!CNFSparse_groups_line()) {
	return FALSE;
    }
    if (cycbufftab == (CYCBUFF *)NULL) {
	syslog(L_ERROR, "%s: zero cycbuffs defined", LocalLogName);
	return FALSE;
    }
    if (metacycbufftab == (METACYCBUFF *)NULL) {
	syslog(L_ERROR, "%s: zero metacycbuffs defined", LocalLogName);
	return FALSE;
    }
    return TRUE;
}

/*
**	Bit arithmetic by brute force.
**
**	XXXYYYXXX WARNING: the code below is not endian-neutral!
*/

typedef unsigned long	ULONG;

STATIC int CNFSUsedBlock(CYCBUFF *cycbuff, CYCBUFF_OFF_T offset,
	      BOOL set_operation, BOOL setbitvalue) {
    CYCBUFF_OFF_T	blocknum;
    CYCBUFF_OFF_T	longoffset;
    int			bitoffset;	/* From the 'left' side of the long */
    static int		uninitialized = 1;
    static int		longsize = sizeof(long);
    int	i;
    ULONG		bitlong, on, off, mask;
    static ULONG	onarray[64], offarray[64];

    if (uninitialized) {
	on = 1;
	off = on;
	off ^= ULONG_MAX;
	for (i = (longsize * 8) - 1; i >= 0; i--) {
	    onarray[i] = on;
	    offarray[i] = off;
	    on <<= 1;
	    off = on;
	    off ^= ULONG_MAX;
	}
	uninitialized = 0;
    }

    /* We allow bit-setting under minartoffset, but it better be FALSE */
    if ((offset < cycbuff->minartoffset && setbitvalue) ||
	offset > cycbuff->len) {
	char	bufoff[64], bufmin[64], bufmax[64];
	SMseterror(SMERR_INTERNAL, NULL);
	strcpy(bufoff, CNFSofft2hex(offset, FALSE));
	strcpy(bufmin, CNFSofft2hex(cycbuff->minartoffset, FALSE));
	strcpy(bufmax, CNFSofft2hex(cycbuff->len, FALSE));
	syslog(L_ERROR,
	       "%s: CNFSUsedBlock: invalid offset %s, min = %s, max = %s",
	       LocalLogName, bufoff, bufmin, bufmax);
	return 0;
    }
    if (offset % CNFS_BLOCKSIZE != 0) {
	SMseterror(SMERR_INTERNAL, NULL);
	syslog(L_ERROR,
	       "%s: CNFSsetusedbitbyrp: offset %s not on %d-byte block boundary",
	       LocalLogName, CNFSofft2hex(offset, FALSE), CNFS_BLOCKSIZE);
	return 0;
    }
    blocknum = offset / CNFS_BLOCKSIZE;
    longoffset = blocknum / (longsize * 8);
    bitoffset = blocknum % (longsize * 8);
    bitlong = *((ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
		+ longoffset);
    if (set_operation) {
	if (setbitvalue) {
	    mask = onarray[bitoffset];
	    bitlong |= mask;
	} else {
	    mask = offarray[bitoffset];
	    bitlong &= mask;
	}
	*((ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
	  + longoffset) = bitlong;
	return 2;	/* XXX Clean up return semantics */
    }
    /* It's a read operation */
    mask = onarray[bitoffset];

/* 
 * return bitlong & mask; doesn't work if sizeof(ulong) > sizeof(int)
 */
    if ( bitlong & mask ) return 1; else return 0;

}

/*
** CNFSmunmapbitfields() -- Call munmap() on all of the bitfields we've
**	previously mmap()'ed.
*/

STATIC void CNFSmunmapbitfields(void) {
    CYCBUFF	*cycbuff;

    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
	if (cycbuff->bitfield != NULL) {
	    munmap(cycbuff->bitfield, cycbuff->minartoffset);
	    cycbuff->bitfield = NULL;
	}
    }
}

STATIC int CNFSArtMayBeHere(CYCBUFF *cycbuff, CYCBUFF_OFF_T offset, U_INT32_T cycnum) {
    STATIC time_t       lastupdate = 0;
    CYCBUFF	        *tmp;

    if (SMpreopen && !SMopenmode) {
	if ((time(NULL) - lastupdate) > refresh_interval) {	/* XXX Changed to refresh every 30sec - cmo*/
	    for (tmp = cycbufftab; tmp != (CYCBUFF *)NULL; tmp = tmp->next) {
		CNFSReadFreeAndCycle(tmp);
	    }
	    lastupdate = time(NULL);
	} else if (cycnum == cycbuff->cyclenum + 1) {	/* rollover ? */
	    CNFSReadFreeAndCycle(cycbuff);
	}
    }
    /*
    ** The current cycle number may have advanced since the last time we
    ** checked it, so use a ">=" check instead of "==".  Our intent is
    ** avoid a false negative response, *not* a false positive response.
    */
    if (! (cycnum == cycbuff->cyclenum ||
	(cycnum == cycbuff->cyclenum - 1 && offset > cycbuff->free) ||
	(cycnum + 1 == 0 && cycbuff->cyclenum == 2 && offset > cycbuff->free))) {
	/* We've been overwritten */
	return 0;
    }
    return CNFSUsedBlock(cycbuff, offset, FALSE, FALSE);
}

STATIC void CNFSshutdowncycbuff(CYCBUFF *cycbuff) {
    if (cycbuff == (CYCBUFF *)NULL)
	return;
    if (cycbuff->needflush)
	(void)CNFSflushhead(cycbuff);
    if (cycbuff->bitfield != NULL) {
	munmap(cycbuff->bitfield, cycbuff->minartoffset);
	cycbuff->bitfield = NULL;
    }
    if (cycbuff->fd >= 0)
	close(cycbuff->fd);
    cycbuff->fd = -1;
}

BOOL cnfs_init(BOOL *selfexpire) {
    int			ret;
    METACYCBUFF	*metacycbuff;
    CYCBUFF	*cycbuff;

    *selfexpire = FALSE;
    if (innconf == NULL) {
	if ((ret = ReadInnConf()) < 0) {
	    syslog(L_ERROR, "%s: ReadInnConf failed, returned %d", LocalLogName, ret);
	    SMseterror(SMERR_INTERNAL, "ReadInnConf() failed");
	    return FALSE;
	}
    }
    if (pagesize == 0) {
#if	defined(HAVE_GETPAGESIZE)
	pagesize = getpagesize();
#elif	defined(_SC_PAGESIZE)
	if ((pagesize = sysconf(_SC_PAGESIZE)) < 0) {
	    syslog(L_ERROR, "%s: sysconf(_SC_PAGESIZE) failed: %m", LocalLogName);
	    SMseterror(SMERR_INTERNAL, "sysconf(_SC_PAGESIZE) failed");
	    return FALSE;
	}
#else
	pagesize = 16384;
#endif
	if ((pagesize > CNFS_HDR_PAGESIZE) || (CNFS_HDR_PAGESIZE % pagesize)) {
	    syslog(L_ERROR, "%s: CNFS_HDR_PAGESIZE (%d) is not a multiple of pagesize (%d)", LocalLogName, CNFS_HDR_PAGESIZE, pagesize);
	    SMseterror(SMERR_INTERNAL, "CNFS_HDR_PAGESIZE not multiple of pagesize");
	    return FALSE;
	}
    }
    if (STORAGE_TOKEN_LENGTH < 16) {
	syslog(L_ERROR, "%s: token length is less than 16 bytes", LocalLogName);
	SMseterror(SMERR_TOKENSHORT, NULL);
	return FALSE;
    }

    if (!CNFSread_config()) {
	CNFScleancycbuff();
	CNFScleanmetacycbuff();
	CNFScleanexpirerule();
	SMseterror(SMERR_INTERNAL, NULL);
	return FALSE;
    }
    if (!CNFSinit_disks(NULL)) {
	CNFScleancycbuff();
	CNFScleanmetacycbuff();
	CNFScleanexpirerule();
	SMseterror(SMERR_INTERNAL, NULL);
	return FALSE;
    }
    for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *)NULL; metacycbuff = metacycbuff->next) {
      metacycbuff->memb_next = 0;
      metacycbuff->write_count = 0;		/* Let's not forget this */
      if (metacycbuff->metamode == SEQUENTIAL)
	/* mark current cycbuff */
	if (CNFS_setcurrent(metacycbuff) == FALSE) {
	  CNFScleancycbuff();
	  CNFScleanmetacycbuff();
	  CNFScleanexpirerule();
	  SMseterror(SMERR_INTERNAL, NULL);
	  return FALSE;
	}
    }
    if (!SMpreopen) {
      for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
	CNFSshutdowncycbuff(cycbuff);
      }
    }

    *selfexpire = TRUE;
    return TRUE;
}

TOKEN cnfs_store(const ARTHANDLE article, const STORAGECLASS class) {
    TOKEN               token;
    CYCBUFF		*cycbuff = NULL;
    METACYCBUFF		*metacycbuff = NULL;
    int			i;
    static char		buf[1024];
    char		*artcycbuffname;
    CYCBUFF_OFF_T	artoffset, middle;
    U_INT32_T		artcyclenum;
    CNFSARTHEADER	cah;
    struct iovec	iov[2];
    int			tonextblock;
    CNFSEXPIRERULES	*metaexprule;

    for (metaexprule = metaexprulestab; metaexprule != (CNFSEXPIRERULES *)NULL; metaexprule = metaexprule->next) {
	if (metaexprule->class == class)
	    break;
    }
    if (metaexprule == (CNFSEXPIRERULES *)NULL) {
	SMseterror(SMERR_INTERNAL, "no rules match");
	syslog(L_ERROR, "%s: no matches for group '%s'",
	       LocalLogName, buf);
	token.type = TOKEN_EMPTY;
	return token;
    }
    metacycbuff = metaexprule->dest;

    cycbuff = metacycbuff->members[metacycbuff->memb_next];  
    if (cycbuff == NULL) {
	SMseterror(SMERR_INTERNAL, "no cycbuff found");
	syslog(L_ERROR, "%s: no cycbuff found for %d", LocalLogName, metacycbuff->memb_next);
	token.type = TOKEN_EMPTY;
	return token;
    } else if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
	SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
	syslog(L_ERROR, "%s: cycbuff '%s' initialization fail", LocalLogName, cycbuff->name);
	token.type = TOKEN_EMPTY;
	return token;
    }
    /* Article too big? */
    if (article.len > cycbuff->len - cycbuff->free - CNFS_BLOCKSIZE - 1) {
	for (middle = cycbuff->free ;middle < cycbuff->len - CNFS_BLOCKSIZE - 1;
	    middle += CNFS_BLOCKSIZE) {
	    CNFSUsedBlock(cycbuff, middle, TRUE, FALSE);
	}
	cycbuff->free = cycbuff->minartoffset;
	cycbuff->cyclenum++;
	if (cycbuff->cyclenum == 0)
	  cycbuff->cyclenum += 2;		/* cnfs_next() needs this */
	cycbuff->needflush = TRUE;
	if (metacycbuff->metamode == INTERLEAVE) {
	  (void)CNFSflushhead(cycbuff);		/* Flush, just for giggles */
	  syslog(L_NOTICE, "%s: cycbuff %s rollover to cycle 0x%x... remain calm",
	       LocalLogName, cycbuff->name, cycbuff->cyclenum);
	} else {
	  /* SEQUENTIAL */
	  cycbuff->currentbuff = FALSE;
	  (void)CNFSflushhead(cycbuff);		/* Flush, just for giggles */
	  if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	  syslog(L_NOTICE, "%s: metacycbuff %s cycbuff is moved to %s remain calm",
	       LocalLogName, metacycbuff->name, cycbuff->name);
	  metacycbuff->memb_next = (metacycbuff->memb_next + 1) % metacycbuff->count;
	  cycbuff = metacycbuff->members[metacycbuff->memb_next];  
	  if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
	      SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
	      syslog(L_ERROR, "%s: cycbuff '%s' initialization fail", LocalLogName, cycbuff->name);
	      token.type = TOKEN_EMPTY;
	      return token;
	  }
	  cycbuff->currentbuff = TRUE;
	  cycbuff->needflush = TRUE;
	  (void)CNFSflushhead(cycbuff);		/* Flush, just for giggles */
	}
    }
    /* Ah, at least we know all three important data */
    artcycbuffname = cycbuff->name;
    artoffset = cycbuff->free;
    artcyclenum = cycbuff->cyclenum;

    memset(&cah, 0, sizeof(cah));
    cah.size = htonl(article.len);
    if (article.arrived == (time_t)0)
	cah.arrived = htonl(time(NULL));
    else
	cah.arrived = htonl(article.arrived);
    cah.class = class;

    if (CNFSseek(cycbuff->fd, artoffset, SEEK_SET) < 0) {
	SMseterror(SMERR_INTERNAL, "CNFSseek() failed");
	syslog(L_ERROR, "%s: CNFSseek failed for '%s' offset 0x%s: %m",
	       LocalLogName, cycbuff->name, CNFSofft2hex(artoffset, FALSE));
	token.type = TOKEN_EMPTY;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return token;
    }
    iov[0].iov_base = (caddr_t) &cah;
    iov[0].iov_len = sizeof(cah);
    iov[1].iov_base = article.data;
    iov[1].iov_len = article.len;
    if (xwritev(cycbuff->fd, iov, 2) < 0) {
	SMseterror(SMERR_INTERNAL, "cnfs_store() xwritev() failed");
	syslog(L_ERROR,
	       "%s: cnfs_store xwritev failed for '%s' offset 0x%s: %m",
	       LocalLogName, artcycbuffname, CNFSofft2hex(artoffset, FALSE));
	token.type = TOKEN_EMPTY;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return token;
    }
    cycbuff->needflush = TRUE;

    /* Now that the article is written, advance the free pointer & flush */
    cycbuff->free += (CYCBUFF_OFF_T) article.len + sizeof(cah);
    tonextblock = CNFS_BLOCKSIZE - (cycbuff->free & (CNFS_BLOCKSIZE - 1));
    cycbuff->free += (CYCBUFF_OFF_T) tonextblock;
    /*
    ** If cycbuff->free > cycbuff->len, don't worry.  The next cnfs_store()
    ** will detect the situation & wrap around correctly.
    */
    if (metacycbuff->metamode == INTERLEAVE)
      metacycbuff->memb_next = (metacycbuff->memb_next + 1) % metacycbuff->count;
    if (++metacycbuff->write_count % metabuff_update == 0) {
	for (i = 0; i < metacycbuff->count; i++) {
	    (void)CNFSflushhead(metacycbuff->members[i]);
	}
    }
    CNFSUsedBlock(cycbuff, artoffset, TRUE, TRUE);
    for (middle = artoffset + CNFS_BLOCKSIZE; middle < cycbuff->free;
	 middle += CNFS_BLOCKSIZE) {
	CNFSUsedBlock(cycbuff, middle, TRUE, FALSE);
    }
    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
    return CNFSMakeToken(artcycbuffname, artoffset, artcyclenum, class, article.token);
}

ARTHANDLE *cnfs_retrieve(const TOKEN token, const RETRTYPE amount) {
    char		cycbuffname[9];
    CYCBUFF_OFF_T	offset;
    U_INT32_T		cycnum;
    CYCBUFF		*cycbuff;
    ARTHANDLE   	*art;
    CNFSARTHEADER	cah;
    PRIV_CNFS		*private;
    char		*p;
    long		pagefudge, blockfudge;
    CYCBUFF_OFF_T	mmapoffset;
    static TOKEN	ret_token;
    static BOOL		nomessage = FALSE;
    CYCBUFF_OFF_T	middle, limit;
    int			plusoffset = 0;

    if (token.type != TOKEN_CNFS) {
	SMseterror(SMERR_INTERNAL, NULL);
	return NULL;
    }
    if (! CNFSBreakToken(token, cycbuffname, &offset, &cycnum)) {
	/* SMseterror() should have already been called */
	return NULL;
    }
    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	SMseterror(SMERR_NOENT, NULL);
	if (!nomessage) {
	    syslog(L_ERROR, "%s: cnfs_retrieve: token %s: bogus cycbuff name: %s:0x%s:%ld",
	       LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
	    nomessage = TRUE;
	}
	return NULL;
    }
    if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
	SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
	syslog(L_ERROR, "%s: cycbuff '%s' initialization fail", LocalLogName, cycbuff->name);
	return NULL;
    }
    if (! CNFSArtMayBeHere(cycbuff, offset, cycnum)) {
	SMseterror(SMERR_NOENT, NULL);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return NULL;
    }

    art = NEW(ARTHANDLE, 1);
    art->type = TOKEN_CNFS;
    if (amount == RETR_STAT) {
	art->data = NULL;
	art->len = 0;
	art->private = NULL;
	ret_token = token;    
	art->token = &ret_token;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    /*
    ** Because we don't know the length of the article (yet), we'll
    ** just mmap() a chunk of memory which is guaranteed to be larger
    ** than the largest article can be.
    ** XXX Because the max article size can be changed, we could get into hot
    ** XXX water here.  So, to be safe, we double MAX_ART_SIZE and add enough
    ** XXX extra for the pagesize fudge factor and CNFSARTHEADER structure.
    */
    if (CNFSseek(cycbuff->fd, offset, SEEK_SET) < 0) {
        SMseterror(SMERR_UNDEFINED, "CNFSseek failed");
        syslog(L_ERROR, "%s: could not lseek token %s %s:0x%s:%ld: %m",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
        DISPOSE(art);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
        return NULL;
    }
    if (read(cycbuff->fd, &cah, sizeof(cah)) != sizeof(cah)) {
        SMseterror(SMERR_UNDEFINED, "read failed");
        syslog(L_ERROR, "%s: could not read token %s %s:0x%s:%ld: %m",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
        DISPOSE(art);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
        return NULL;
    }
#ifdef OLD_CNFS
    if(cah.size == htonl(0x1234) && ntohl(cah.arrived) < time(NULL)-10*365*24*3600) {
	oldCNFSARTHEADER cahh;
	*(CNFSARTHEADER *)&cahh = cah;
	if(read(cycbuff->fd, ((char *)&cahh)+sizeof(CNFSARTHEADER), sizeof(oldCNFSARTHEADER)-sizeof(CNFSARTHEADER)) != sizeof(oldCNFSARTHEADER)-sizeof(CNFSARTHEADER)) {
            SMseterror(SMERR_UNDEFINED, "read2 failed");
            syslog(L_ERROR, "%s: could not read2 token %s %s:0x%s:%ld: %m",
                    LocalLogName, TokenToText(token), cycbuffname,
                    CNFSofft2hex(offset, FALSE), cycnum);
            DISPOSE(art);
            if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
            return NULL;
	}
	cah.size = cahh.size;
	cah.arrived = htonl(time(NULL));
	cah.class = 0;
	plusoffset = sizeof(oldCNFSARTHEADER)-sizeof(CNFSARTHEADER);
    }
#endif /* OLD_CNFS */
    if (offset > cycbuff->len - CNFS_BLOCKSIZE - ntohl(cah.size) - 1) {
        if (!SMpreopen) {
	    SMseterror(SMERR_UNDEFINED, "CNFSARTHEADER size overflow");
	    syslog(L_ERROR, "%s: could not match article size token %s %s:0x%s:%ld: %ld",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum, ntohl(cah.size));
	    DISPOSE(art);
	    CNFSshutdowncycbuff(cycbuff);
	    return NULL;
	}
	CNFSReadFreeAndCycle(cycbuff);
	if (offset > cycbuff->len - CNFS_BLOCKSIZE - ntohl(cah.size) - 1) {
	    SMseterror(SMERR_UNDEFINED, "CNFSARTHEADER size overflow");
	    syslog(L_ERROR, "%s: could not match article size token %s %s:0x%s:%ld: %ld",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum, ntohl(cah.size));
	    DISPOSE(art);
	    return NULL;
	}
    }
    /* check the bitmap to ensure cah.size is not broken */
    /* cannot believe cycbuff->free, since it may not be updated by writer */
    blockfudge = (sizeof(cah) + plusoffset + ntohl(cah.size)) % CNFS_BLOCKSIZE;
    limit = offset + sizeof(cah) + plusoffset + ntohl(cah.size) - blockfudge + CNFS_BLOCKSIZE;
    for (middle = offset + CNFS_BLOCKSIZE; middle < limit;
	middle += CNFS_BLOCKSIZE) {
	if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) != 0)
	/* Bitmap set.  This article assumes to be broken */
	break;
    }
    if (middle != limit) {
	char buf1[24], buf2[24], buf3[24];
	strcpy(buf1, CNFSofft2hex(cycbuff->free, FALSE));
	strcpy(buf2, CNFSofft2hex(middle, FALSE));
	strcpy(buf3, CNFSofft2hex(limit, FALSE));
	SMseterror(SMERR_UNDEFINED, "size overflows bitmaps");
	syslog(L_ERROR, "%s: size overflows bitmaps %s %s:0x%s:0x%s:0x%s: %ld",
	LocalLogName, TokenToText(token), cycbuffname, buf1, buf2, buf3, ntohl(cah.size));
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	DISPOSE(art);
	return NULL;
    }
    if ((innconf->cnfscheckfudgesize != 0) && (ntohl(cah.size) > innconf->maxartsize + innconf->cnfscheckfudgesize)) {
	char buf1[24], buf2[24], buf3[24];
	strcpy(buf1, CNFSofft2hex(cycbuff->free, FALSE));
	strcpy(buf2, CNFSofft2hex(middle, FALSE));
	strcpy(buf3, CNFSofft2hex(limit, FALSE));
	SMseterror(SMERR_UNDEFINED, "CNFSARTHEADER fudge size overflow");
	syslog(L_ERROR, "%s: fudge size overflows bitmaps %s %s:0x%s:0x%s:0x%s: %ld",
	LocalLogName, TokenToText(token), cycbuffname, buf1, buf2, buf3, ntohl(cah.size));
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	DISPOSE(art);
	return NULL;
    }
    private = NEW(PRIV_CNFS, 1);
    art->private = (void *)private;
    art->arrived = ntohl(cah.arrived);
    if (innconf->articlemmap) {
	offset += sizeof(cah) + plusoffset;
	pagefudge = offset % pagesize;
	mmapoffset = offset - pagefudge;
	private->len = pagefudge + ntohl(cah.size);
	if ((private->base = mmap((MMAP_PTR)0, private->len, PROT_READ,
		MAP__ARG, cycbuff->fd, mmapoffset)) == (MMAP_PTR) -1) {
	    SMseterror(SMERR_UNDEFINED, "mmap failed");
	    syslog(L_ERROR, "%s: could not mmap token %s %s:0x%s:%ld: %m",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
	    DISPOSE(art->private);
	    DISPOSE(art);
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    return NULL;
	}
#if defined(MADV_SEQUENTIAL) && defined(HAVE_MADVISE)
	madvise(private->base, private->len, MADV_SEQUENTIAL);
#endif
    } else {
	private->base = NEW(char, ntohl(cah.size));
	pagefudge = 0;
	if (read(cycbuff->fd, private->base, ntohl(cah.size)) < 0) {
	    SMseterror(SMERR_UNDEFINED, "read failed");
	    syslog(L_ERROR, "%s: could not read token %s %s:0x%s:%ld: %m",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
	    DISPOSE(private->base);
	    DISPOSE(art->private);
	    DISPOSE(art);
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    return NULL;
	}
    }
    ret_token = token;    
    art->token = &ret_token;
    art->len = ntohl(cah.size);
    if (amount == RETR_ALL) {
	art->data = innconf->articlemmap ? private->base + pagefudge : private->base;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    if ((p = SMFindBody(innconf->articlemmap ? private->base + pagefudge : private->base, art->len)) == NULL) {
        SMseterror(SMERR_NOBODY, NULL);
	if (innconf->articlemmap) {
#if defined(MADV_DONTNEED) && defined(HAVE_MADVISE)
	    madvise(private->base, private->len, MADV_DONTNEED);
#endif
	    munmap(private->base, private->len);
	} else
	    DISPOSE(private->base);
        DISPOSE(art->private);
        DISPOSE(art);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
        return NULL;
    }
    if (amount == RETR_HEAD) {
	if (innconf->articlemmap) {
	    art->data = private->base + pagefudge;
            art->len = p - private->base - pagefudge;
	} else {
	    art->data = private->base;
            art->len = p - private->base;
	}
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    if (amount == RETR_BODY) {
        art->data = p;
	if (innconf->articlemmap)
	    art->len = art->len - (p - private->base - pagefudge);
	else
	    art->len = art->len - (p - private->base);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    if (innconf->articlemmap) {
#if defined(MADV_DONTNEED) && defined(HAVE_MADVISE)
	madvise(private->base, private->len, MADV_DONTNEED);
#endif
	munmap(private->base, private->len);
    } else
	DISPOSE(private->base);
    DISPOSE(art->private);
    DISPOSE(art);
    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
    return NULL;
}

void cnfs_freearticle(ARTHANDLE *article) {
    PRIV_CNFS	*private;

    if (!article)
	return;
    
    if (article->private) {
	private = (PRIV_CNFS *)article->private;
	if (innconf->articlemmap) {
#if defined(MADV_DONTNEED) && defined(HAVE_MADVISE)
	    madvise(private->base, private->len, MADV_DONTNEED);
#endif	/* MADV_DONTNEED */
	    munmap(private->base, private->len);
	} else
	    DISPOSE(private->base);
	DISPOSE(private);
    }
    DISPOSE(article);
}

BOOL cnfs_cancel(TOKEN token) {
    char		cycbuffname[9];
    CYCBUFF_OFF_T	offset;
    U_INT32_T		cycnum;
    CYCBUFF		*cycbuff;

    if (token.type != TOKEN_CNFS) {
	SMseterror(SMERR_INTERNAL, NULL);
	return FALSE;
    }
    if (! CNFSBreakToken(token, cycbuffname, &offset, &cycnum)) {
	SMseterror(SMERR_INTERNAL, NULL);
	/* SMseterror() should have already been called */
	return FALSE;
    }
    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	return FALSE;
    }
    if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
	SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
	syslog(L_ERROR, "%s: cycbuff '%s' initialization fail", LocalLogName, cycbuff->name);
	return FALSE;
    }
    if (! (cycnum == cycbuff->cyclenum ||
	(cycnum == cycbuff->cyclenum - 1 && offset > cycbuff->free) ||
	(cycnum + 1 == 0 && cycbuff->cyclenum == 2 && offset > cycbuff->free))) {
	SMseterror(SMERR_NOENT, NULL);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return FALSE;
    }
    if (CNFSUsedBlock(cycbuff, offset, FALSE, FALSE) == 0) {
	SMseterror(SMERR_NOENT, NULL);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return FALSE;
    }
    CNFSUsedBlock(cycbuff, offset, TRUE, FALSE);
    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
    return TRUE;
}

ARTHANDLE *cnfs_next(const ARTHANDLE *article, const RETRTYPE amount) {
    ARTHANDLE           *art;
    CYCBUFF		*cycbuff;
    PRIV_CNFS		priv, *private;
    CYCBUFF_OFF_T	middle, limit;
    CNFSARTHEADER	cah;
    CYCBUFF_OFF_T	offset;
    long		pagefudge, blockfudge;
    static TOKEN	token;
    int			tonextblock;
    CYCBUFF_OFF_T	mmapoffset;
    char		*p;
    int			plusoffset = 0;

    if (article == (ARTHANDLE *)NULL) {
	if ((cycbuff = cycbufftab) == (CYCBUFF *)NULL)
	    return (ARTHANDLE *)NULL;
	priv.offset = 0;
	priv.rollover = FALSE;
    } else {        
	priv = *(PRIV_CNFS *)article->private;
	DISPOSE(article->private);
	DISPOSE(article);
	if (innconf->articlemmap) {
#if defined(MADV_DONTNEED) && defined(HAVE_MADVISE)
	    madvise(priv.base, priv.len, MADV_DONTNEED);  
#endif
	    munmap(priv.base, priv.len);
	} else {
	    /* In the case we return art->data = NULL, we
             * must not free an already stale pointer.
               -mibsoft@mibsoftware.com
	     */
	    if (priv.base) {
	        DISPOSE(priv.base);
		priv.base = 0;
	    }
	}
	cycbuff = priv.cycbuff;
    }

    for (;cycbuff != (CYCBUFF *)NULL;
    	    cycbuff = cycbuff->next,
	    priv.offset = 0) {

	if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
	    SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
	    continue;
	}
	if (priv.rollover && priv.offset >= cycbuff->free) {
	    priv.offset = 0;
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    continue;
	}
	if (priv.offset == 0) {
	    if (cycbuff->cyclenum == 1) {
	    	priv.offset = cycbuff->minartoffset;
		priv.rollover = TRUE;
	    } else {
	    	priv.offset = cycbuff->free;
		priv.rollover = FALSE;
	    }
	}
	if (!priv.rollover) {
	    for (middle = priv.offset ;middle < cycbuff->len - CNFS_BLOCKSIZE - 1;
		middle += CNFS_BLOCKSIZE) {
		if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) != 0)
		    break;
	    }
	    if (middle >= cycbuff->len - CNFS_BLOCKSIZE - 1) {
		priv.rollover = TRUE;
		middle = cycbuff->minartoffset;
	    }
	    break;
	} else {
	    for (middle = priv.offset ;middle < cycbuff->free;
		middle += CNFS_BLOCKSIZE) {
		if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) != 0)
		    break;
	    }
	    if (middle >= cycbuff->free) {
		middle = 0;
		if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
		continue;
	    } else
		break;
	}
    }
    if (cycbuff == (CYCBUFF *)NULL)
	return (ARTHANDLE *)NULL;

    offset = middle;
    if (CNFSseek(cycbuff->fd, offset, SEEK_SET) < 0) {
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return (ARTHANDLE *)NULL;
    }
    if (read(cycbuff->fd, &cah, sizeof(cah)) != sizeof(cah)) {
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return (ARTHANDLE *)NULL;
    }
#ifdef OLD_CNFS
    if(cah.size == htonl(0x1234) && ntohl(cah.arrived) < time(NULL)-10*365*24*3600) {
	oldCNFSARTHEADER cahh;
	*(CNFSARTHEADER *)&cahh = cah;
	if(read(cycbuff->fd, ((char *)&cahh)+sizeof(CNFSARTHEADER), sizeof(oldCNFSARTHEADER)-sizeof(CNFSARTHEADER)) != sizeof(oldCNFSARTHEADER)-sizeof(CNFSARTHEADER)) {
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    return (ARTHANDLE *)NULL;
	}
	cah.size = cahh.size;
	cah.arrived = htonl(time(NULL));
	cah.class = 0;
	plusoffset = sizeof(oldCNFSARTHEADER)-sizeof(CNFSARTHEADER);
    }
#endif /* OLD_CNFS */
    art = NEW(ARTHANDLE, 1);
    private = NEW(PRIV_CNFS, 1);
    art->private = (void *)private;
    art->type = TOKEN_CNFS;
    *private = priv;
    private->cycbuff = cycbuff;
    private->offset = middle;
    if (cycbuff->free + ntohl(cah.size) > cycbuff->len - CNFS_BLOCKSIZE - 1) {
	private->offset += CNFS_BLOCKSIZE;
	art->data = NULL;
	art->len = 0;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    /* check the bitmap to ensure cah.size is not broken */
    blockfudge = (sizeof(cah) + plusoffset + ntohl(cah.size)) % CNFS_BLOCKSIZE;
    limit = private->offset + sizeof(cah) + plusoffset + ntohl(cah.size) - blockfudge + CNFS_BLOCKSIZE;
    if (offset < cycbuff->free) {
	for (middle = offset + CNFS_BLOCKSIZE; (middle < cycbuff->free) && (middle < limit);
	    middle += CNFS_BLOCKSIZE) {
	    if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) != 0)
		/* Bitmap set.  This article assumes to be broken */
		break;
	}
	if ((middle >= cycbuff->free) || (middle != limit)) {
	    private->offset = middle;
	    art->data = NULL;
	    art->len = 0;
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    return art;
	}
    } else {
	for (middle = offset + CNFS_BLOCKSIZE; (middle < cycbuff->len) && (middle < limit);
	    middle += CNFS_BLOCKSIZE) {
	    if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) != 0)
		/* Bitmap set.  This article assumes to be broken */
		break;
	}
	if ((middle >= cycbuff->len) || (middle != limit)) {
	    private->offset = middle;
	    art->data = NULL;
	    art->len = 0;
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    return art;
	}
    }
    if ((innconf->cnfscheckfudgesize != 0) && (ntohl(cah.size) > innconf->maxartsize + innconf->cnfscheckfudgesize)) {
	art->data = NULL;
	art->len = 0;
	private->base = 0;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }

    private->offset += (CYCBUFF_OFF_T) ntohl(cah.size) + sizeof(cah) + plusoffset;
    tonextblock = CNFS_BLOCKSIZE - (private->offset & (CNFS_BLOCKSIZE - 1));
    private->offset += (CYCBUFF_OFF_T) tonextblock;
    art->arrived = ntohl(cah.arrived);
    token = CNFSMakeToken(cycbuff->name, offset, (offset > cycbuff->free) ? cycbuff->cyclenum - 1 : cycbuff->cyclenum, cah.class, (TOKEN *)NULL);
    art->token = &token;
    if (innconf->articlemmap) {
	offset += sizeof(cah) + plusoffset;
	pagefudge = offset % pagesize;
	mmapoffset = offset - pagefudge;
	private->len = pagefudge + ntohl(cah.size);
	if ((private->base = mmap((MMAP_PTR)0, private->len, PROT_READ,
	    MAP__ARG, cycbuff->fd, mmapoffset)) == (MMAP_PTR) -1) {
	    art->data = NULL;
	    art->len = 0;
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    return art;
	}
#if defined(MADV_SEQUENTIAL) && defined(HAVE_MADVISE)
	madvise(private->base, private->len, MADV_SEQUENTIAL);
#endif
    } else {
	private->base = NEW(char, ntohl(cah.size));
	pagefudge = 0;
	if (read(cycbuff->fd, private->base, ntohl(cah.size)) < 0) {
	    art->data = NULL;
	    art->len = 0;
	    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	    DISPOSE(private->base);
	    private->base = 0;
	    return art;
	}
    }
    art->len = ntohl(cah.size);
    if (amount == RETR_ALL) {
	art->data = innconf->articlemmap ? private->base + pagefudge : private->base;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    if ((p = SMFindBody(innconf->articlemmap ? private->base + pagefudge : private->base, art->len)) == NULL) {
	art->data = NULL;
	art->len = 0;
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    if (amount == RETR_HEAD) {
	if (innconf->articlemmap) {
	    art->data = private->base + pagefudge;
	    art->len = p - private->base - pagefudge;
	} else {
	    art->data = private->base;
	    art->len = p - private->base;
	}
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    if (amount == RETR_BODY) {
	art->data = p;
	if (innconf->articlemmap)
	    art->len = art->len - (p - private->base - pagefudge);
	else
	    art->len = art->len - (p - private->base);
	if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
	return art;
    }
    art->data = NULL;
    art->len = 0;
    if (!SMpreopen) CNFSshutdowncycbuff(cycbuff);
    return art;
}

BOOL cnfs_ctl(PROBETYPE type, TOKEN *token, void *value) {
    struct artngnum *ann;

    switch (type) {
    case SMARTNGNUM:    
	if ((ann = (struct artngnum *)value) == NULL)
	    return FALSE;
	/* make SMprobe() call cnfs_retrieve() */
	ann->artnum = 0;
	return TRUE; 
    default:
	return FALSE; 
    }   
}

void cnfs_shutdown(void) {
    CNFSflushallheads();
    CNFSmunmapbitfields();
    CNFScleancycbuff();
    CNFScleanmetacycbuff();
    CNFScleanexpirerule();
}
