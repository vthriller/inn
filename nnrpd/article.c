/*  $Revision$
**
**  Article-related routines.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/uio.h>

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#include "nnrpd.h"
#include "ov.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include "tls.h"
extern SSL *tls_conn;
#endif 

/*
**  Data structures for use in ARTICLE/HEAD/BODY/STAT common code.
*/
typedef enum _SENDTYPE {
    STarticle,
    SThead,
    STbody,
    STstat
} SENDTYPE;

typedef struct _SENDDATA {
    SENDTYPE	Type;
    int		ReplyCode;
    STRING	Item;
} SENDDATA;

STATIC char		ARTnotingroup[] = NNTP_NOTINGROUP;
STATIC char		ARTnoartingroup[] = NNTP_NOARTINGRP;
STATIC char		ARTnocurrart[] = NNTP_NOCURRART;
STATIC ARTHANDLE        *ARThandle = NULL;
STATIC int              ARTxreffield = 0;
STATIC SENDDATA		SENDbody = {
    STbody,	NNTP_BODY_FOLLOWS_VAL,		"body"
};
STATIC SENDDATA		SENDarticle = {
    STarticle,	NNTP_ARTICLE_FOLLOWS_VAL,	"article"
};
STATIC SENDDATA		SENDstat = {
    STstat,	NNTP_NOTHING_FOLLOWS_VAL,	"status"
};
STATIC SENDDATA		SENDhead = {
    SThead,	NNTP_HEAD_FOLLOWS_VAL,		"head"
};


STATIC struct iovec	iov[IOV_MAX];
STATIC int		queued_iov = 0;

BOOL PushIOvRateLimited(void) {
    struct timeval      start, end;
    struct iovec        newiov[IOV_MAX];
    int                 newiov_len;
    int                 sentiov = 0;
    int                 i;
    int                 bytesfound;
    int                 chunkbittenoff;
    struct timeval      waittime;
    int                 targettime;

    while (queued_iov) {
	bytesfound = newiov_len = 0;
	for (i = 0; (i < queued_iov) && (bytesfound < MaxBytesPerSecond); i++) {
	    if (iov[i].iov_len + bytesfound > MaxBytesPerSecond) {
		chunkbittenoff = MaxBytesPerSecond - bytesfound;
		newiov[newiov_len].iov_base = iov[i].iov_base;
		newiov[newiov_len++].iov_len = chunkbittenoff;
		iov[i].iov_base = (char *)iov[i].iov_base + chunkbittenoff;
		iov[i].iov_len -= chunkbittenoff;
		bytesfound += chunkbittenoff;
	    } else {
		newiov[newiov_len++] = iov[i];
		sentiov++;
		bytesfound += iov[i].iov_len;
	    }
	}
	gettimeofday(&start, NULL);
	if (writev(STDOUT_FILENO, newiov, newiov_len) <= 0) {
	    queued_iov = 0;
	    return FALSE;
	}
	gettimeofday(&end, NULL);
	/* Normalize it so we can just do straight subtraction */
	if (end.tv_usec < start.tv_usec) {
	    end.tv_usec += 1000000;
	    end.tv_sec -= 1;
	}
	waittime.tv_usec = end.tv_usec - start.tv_usec;
	waittime.tv_sec = end.tv_sec - start.tv_sec;
	targettime = (float)1000000 * (float)bytesfound / (float)MaxBytesPerSecond;
	if ((waittime.tv_sec < 1) && (waittime.tv_usec < targettime)) {
	    waittime.tv_usec = targettime - waittime.tv_usec;
	    if (select(0, NULL, NULL, NULL, &waittime) != 0)
		syslog(L_ERROR, "%s: select in PushIOvRateLimit failed %m(%d)",
		       ClientHost, errno);
	}
	memmove(iov, &iov[sentiov], (queued_iov - sentiov) * sizeof(struct iovec));
	queued_iov -= sentiov;
    }
    return TRUE;
}

BOOL PushIOv(void) {
    fflush(stdout);
#ifdef HAVE_SSL
    if (tls_conn) {
      if (SSL_writev(tls_conn, iov, queued_iov) <= 0) {
        queued_iov = 0;
	return FALSE;
      }
    } else {
      if (writev(STDOUT_FILENO, iov, queued_iov) <= 0) {
        queued_iov = 0;
	return FALSE;
      }
    }
#else
    if (MaxBytesPerSecond != 0)
	return PushIOvRateLimited();
    if (writev(STDOUT_FILENO, iov, queued_iov) <= 0) {
      queued_iov = 0;
      return FALSE;
    }
#endif
    queued_iov = 0;
    return TRUE;
}

BOOL SendIOv(char *p, int len) {
    char                *q;

    if (queued_iov) {
	q = (char *)iov[queued_iov - 1].iov_base + iov[queued_iov - 1].iov_len;
	if (p == q) {
	    iov[queued_iov - 1].iov_len += len;
	    return TRUE;
	}
    }
    iov[queued_iov].iov_base = p;
    iov[queued_iov++].iov_len = len;
    if (queued_iov == IOV_MAX)
        return PushIOv();
    return TRUE;
}

STATIC char		*_IO_buffer_ = NULL;
STATIC int		highwater = 0;

BOOL PushIOb(void) {
    fflush(stdout);
#ifdef HAVE_SSL
    if (tls_conn) {
      if (SSL_write(tls_conn, _IO_buffer_, highwater) != highwater) {
        highwater = 0;
        return FALSE;
      }
    } else {
      if (write(STDOUT_FILENO, _IO_buffer_, highwater) != highwater) {
	highwater = 0;
	return FALSE;
      }
    }
#else
    if (write(STDOUT_FILENO, _IO_buffer_, highwater) != highwater) {
        highwater = 0;
        return FALSE;
    }
#endif
    highwater = 0;
    return TRUE;
}

BOOL SendIOb(char *p, int len) {
    int tocopy;
    
    if (_IO_buffer_ == NULL)
        _IO_buffer_ = NEW(char, BIG_BUFFER);

    while (len > 0) {
        tocopy = (len > (BIG_BUFFER - highwater)) ? (BIG_BUFFER - highwater) : len;
        memcpy(&_IO_buffer_[highwater], p, tocopy);
        p += tocopy;
        highwater += tocopy;
        len -= tocopy;
        if (highwater == BIG_BUFFER)
            PushIOb();
    }
    return TRUE;
}


/*
**  Read the overview schema.
*/
BOOL ARTreadschema(void)
{
    static char			*SCHEMA = NULL;
    FILE			*F;
    char			*p;
    ARTOVERFIELD		*fp;
    int				i;
    char			buff[SMBUF];
    BOOL			foundxref = FALSE;
    BOOL			foundxreffull = FALSE;

    /* Open file, count lines. */
    if (SCHEMA == NULL)
	SCHEMA = COPY(cpcatpath(innconf->pathetc, _PATH_SCHEMA));
    if ((F = fopen(SCHEMA, "r")) == NULL) {
	syslog(L_ERROR, "%s Can't open %s, %s", ClientHost, SCHEMA, strerror(errno));
	return FALSE;
    }
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;
    fseeko(F, 0, SEEK_SET);
    ARTfields = NEW(ARTOVERFIELD, i + 1);

    /* Parse each field. */
    for (fp = ARTfields; fgets(buff, sizeof buff, F) != NULL; ) {
	/* Ignore blank and comment lines. */
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, COMMENT_CHAR)) != NULL)
	    *p = '\0';
	if (buff[0] == '\0')
	    continue;
	if ((p = strchr(buff, ':')) != NULL) {
	    *p++ = '\0';
	    fp->NeedsHeader = EQ(p, "full");
	}
	else
	    fp->NeedsHeader = FALSE;
	fp->Header = COPY(buff);
	fp->Length = strlen(buff);
	if (caseEQ(buff, "Xref")) {
	    foundxref = TRUE;
	    foundxreffull = fp->NeedsHeader;
	    fp++;
	    ARTxreffield = fp - ARTfields - 1;
	    fp->Header = COPY("Newsgroups");
	    fp->Length = strlen("Newsgroups");
	    continue;
	}
	fp++;
    }
    ARTfieldsize = fp - ARTfields;
    (void)fclose(F);
    if (!foundxref || !foundxreffull) {
	syslog(L_ERROR, "%s 'Xref:full' must be included in %s", ClientHost, SCHEMA);
	return FALSE;
    }
    return TRUE;
}


/*
**  If we have an article open, close it.
*/
void ARTclose(void)
{
    if (ARThandle) {
	SMfreearticle(ARThandle);
	ARThandle = NULL;
    }
}

BOOL ARTinstorebytoken(TOKEN token)
{
    ARTHANDLE *art;
    struct timeval	stv, etv;

    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&stv, NULL);
    }
    art = SMretrieve(token, RETR_STAT); /* XXX This isn't really overstats, is it? */
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERartcheck+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERartcheck+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if (art) {
	SMfreearticle(art);
	return TRUE;
    } 
    return FALSE;
}

STATIC BOOL ARTinstorebyartnum(int artnum)
{
    ARTHANDLE           *art;
    struct timeval	stv, etv;
    TOKEN               token;
    
    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);
    if (!OVgetartinfo(GRPcur, artnum, NULL, NULL, &token))
	return FALSE;
  
    art = SMretrieve(token, RETR_STAT);
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERartcheck+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERartcheck+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if (art) {
	SMfreearticle(art);
	return TRUE;
    }
    return FALSE;
}

/*
**  If the article name is valid, open it and stuff in the ID.
*/
STATIC BOOL ARTopen(int artnum)
{
    static ARTNUM	save_artnum;
    TOKEN		token;

    /* Re-use article if it's the same one. */
    if (save_artnum == artnum) {
	if (ARThandle)
	    return TRUE;
    }
    ARTclose();

    if (!OVgetartinfo(GRPcur, artnum, NULL, NULL, &token))
	return FALSE;
  
    if ((ARThandle = SMretrieve(token, RETR_ALL)) == NULL)
	return FALSE;

    save_artnum = artnum;
    return TRUE;
}


/*
**  Open the article for a given Message-ID.
*/
STATIC BOOL ARTopenbyid(char *msg_id, ARTNUM *ap)
{
    char		*p;
    HASH		hash = HashMessageID(msg_id);
    TOKEN		token;

    *ap = 0;
    if ((p = HISgetent(&hash, FALSE, NULL)) == NULL)
	return FALSE;

    token = TextToToken(p);
    if (token.type == TOKEN_EMPTY)
	return FALSE;
    if ((ARThandle = SMretrieve(token, RETR_ALL)) == NULL)
	return FALSE;

    return TRUE;
}

/*
**  Send a (part of) a file to stdout, doing newline and dot conversion.
*/
STATIC void ARTsendmmap(SENDTYPE what)
{
    char		*p, *q, *r, *s, *path, *xref, *virtualpath;
    struct timeval	stv, etv;
    long		bytecount;
    char		lastchar;

    ARTcount++;
    GRParticles++;
    bytecount = 0;
    lastchar = -1;

     gettimeofday(&stv, NULL);
    /* Get the headers and detect if wire format. */
    if (what == STarticle) {
	q = ARThandle->data;
	p = ARThandle->data + ARThandle->len;
     } else {
	for (q = p = ARThandle->data; p < (ARThandle->data + ARThandle->len); p++) {
	    if (*p == '\r')
		continue;
	    if (*p == '\n') {
		if (lastchar == '\n') {
		    if (what == SThead) {
			if (*(p-1) == '\r')
			    p--;
			break;
		    } else {
			q = p + 1;
			p = ARThandle->data + ARThandle->len;
			break;
		    }
		}
	    }
	    lastchar = *p;
	}
    }

    if (VirtualPathlen > 0 && (what != STbody)) {
	if ((path = (char *)HeaderFindMem(ARThandle->data, ARThandle->len, "path", sizeof("path") - 1)) == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    gettimeofday(&etv, NULL);
	    ARTget++;
	    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
	    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
	    return;
	} else if ((xref = (char *)HeaderFindMem(ARThandle->data, ARThandle->len, "xref", sizeof("xref") - 1)) == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    gettimeofday(&etv, NULL);
	    ARTget++;
	    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
	    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
	    return;
	}
	if ((r = memchr(xref, ' ', q - xref)) == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    gettimeofday(&etv, NULL);
	    ARTget++;
	    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
	    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
	    return;
	} else {
	    for (; (r < q) && isspace((int)*r) ; r++);
	    if (r == q) {
		SendIOv(".\r\n", 3);
		ARTgetsize += 3;
		PushIOv();
		gettimeofday(&etv, NULL);
		ARTget++;
		ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
		ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
		return;
	    }
	}
	virtualpath = NEW(char, VirtualPathlen + 2);
	sprintf(virtualpath, "!%s", VirtualPath);
	for (s = path ; s + VirtualPathlen + 1 < ARThandle->data + ARThandle->len ; s++) {
	    if (*s != *virtualpath || !EQn(s, virtualpath, VirtualPathlen + 1))
		continue;
	    break;
	}
	if (s + VirtualPathlen + 1 < ARThandle->data + ARThandle->len) {
	    if (xref > path) {
	        SendIOv(q, path - q);
	        SendIOv(s + 1, xref - (s + 1));
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, p - r);
	    } else {
	        SendIOv(q, xref - q);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(xref, path - xref);
	        SendIOv(s + VirtualPathlen, p - (s + VirtualPathlen));
	    }
	} else {
	    if (xref > path) {
	        SendIOv(q, path - q);
	        SendIOv(VirtualPath, VirtualPathlen);
	        SendIOv(path, xref - path);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, p - r);
	    } else {
	        SendIOv(q, xref - q);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(xref, path - xref);
	        SendIOv(VirtualPath, VirtualPathlen);
	        SendIOv(path, p - path);
	    }
	}
	DISPOSE(virtualpath);
    } else
	SendIOv(q, p - q);
    ARTgetsize += p - q;
    if (what == SThead) {
	SendIOv(".\r\n", 3);
	ARTgetsize += 3;
    } else if (memcmp((ARThandle->data + ARThandle->len - 5), "\r\n.\r\n", 5)) {
	if (memcmp((ARThandle->data + ARThandle->len - 2), "\r\n", 2)) {
	    SendIOv("\r\n.\r\n", 5);
	    ARTgetsize += 5;
	} else {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	}
    }
    PushIOv();

    gettimeofday(&etv, NULL);
    ARTget++;
    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
}

/*
**  Return the header from the specified file, or NULL if not found.
**  We can estimate the Lines header, if that's what's wanted.
*/
char *GetHeader(char *header, BOOL IsLines)
{
    static char		buff[40];
    char		*p, *q, *r, *s, *virtualpath;
    /* Bogus value here to make sure that it isn't initialized to \n */
    char		lastchar = ' ';
    char		*limit;
    static char		*retval = NULL;
    static int		retlen = 0;
    int			headerlen;
    BOOL		pathheader = FALSE;
    BOOL		xrefheader = FALSE;

    limit = ARThandle->data + ARThandle->len - strlen(header);
    for (p = ARThandle->data; p < limit; p++) {
	if (*p == '\r')
	    continue;
	if ((lastchar == '\n') && (*p == '\n')) {
	    return NULL;
	}
	if ((lastchar == '\n') || (p == ARThandle->data)) {
	    headerlen = strlen(header);
	    if (caseEQn(p, header, headerlen)) {
		for (; (p < limit) && !isspace((int)*p) ; p++);
		for (; (p < limit) && isspace((int)*p) ; p++);
		for (q = p; q < limit; q++) 
		    if ((*q == '\r') || (*q == '\n'))
			break;
		if (q == limit)
		    return NULL;
		if (caseEQn("Path", header, headerlen))
		    pathheader = TRUE;
		else if (caseEQn("Xref", header, headerlen))
		    xrefheader = TRUE;
		if (retval == NULL) {
		    retlen = q - p + VirtualPathlen + 1;
		    retval = NEW(char, retlen);
		} else {
		    if ((q - p + VirtualPathlen + 1) > retlen) {
			retlen = q - p + VirtualPathlen + 1;
			RENEW(retval, char, retlen);
		    }
		}
		if (pathheader && (VirtualPathlen > 0)) {
		    virtualpath = NEW(char, VirtualPathlen + 1);
		    sprintf(virtualpath, "!%s", VirtualPath);
		    for (s = p ; s + VirtualPathlen + 1 < ARThandle->data + ARThandle->len ; s++) {
			if (*s != *virtualpath || !EQn(s, virtualpath, VirtualPathlen + 1))
			    continue;
			break;
		    }
		    if (s + VirtualPathlen + 1 < ARThandle->data + ARThandle->len) {
			memcpy(retval, s + 1, q - (s + 1));
		    } else {
			memcpy(retval, VirtualPath, VirtualPathlen);
			memcpy(retval + VirtualPathlen, p, q - p);
			*(retval + (int)(q - p) + VirtualPathlen) = '\0';
		    }
		    DISPOSE(virtualpath);
		} else if (xrefheader && (VirtualPathlen > 0)) {
		    if ((r = memchr(p, ' ', q - p)) == NULL)
			return NULL;
		    for (; (r < q) && isspace((int)*r) ; r++);
		    if (r == q)
			return NULL;
		    memcpy(retval, VirtualPath, VirtualPathlen - 1);
		    memcpy(retval + VirtualPathlen - 1, r, q - r);
		    *(retval + (int)(q - r) + VirtualPathlen - 1) = '\0';
		} else {
		    memcpy(retval, p, q - p);
		    *(retval + (int)(q - p)) = '\0';
		}
		return retval;
	    }
	}
	lastchar = *p;
    }

    if (IsLines) {
	/* Lines estimation taken from Tor Lillqvist <tml@tik.vtt.fi>'s
	 * posting <TML.92Jul10031233@hemuli.tik.vtt.fi> in
	 * news.sysadmin. */
	(void)sprintf(buff, "%d",
		      (int)(6.4e-8 * ARThandle->len * ARThandle->len + 0.023 * ARThandle->len - 12));
	return buff;
    }
    return NULL;
}

/*
**  Fetch part or all of an article and send it to the client.
*/
FUNCTYPE CMDfetch(int ac, char *av[])
{
    char		buff[SMBUF];
    SENDDATA		*what;
    BOOL		ok;
    ARTNUM		art;
    char		*msgid;
    ARTNUM		tart;

    /* Find what to send; get permissions. */
    ok = PERMcanread;
    switch (*av[0]) {
    default:
	what = &SENDbody;
	break;
    case 'a': case 'A':
	what = &SENDarticle;
	break;
    case 's': case 'S':
	what = &SENDstat;
	break;
    case 'h': case 'H':
	what = &SENDhead;
	/* Poster might do a "head" command to verify the article. */
	ok = PERMcanread || PERMcanpost;
	break;
    }

    if (!ok) {
	Reply("%s\r\n", NOACCESS);
	return;
    }

    /* Requesting by Message-ID? */
    if (ac == 2 && av[1][0] == '<') {
	if (!ARTopenbyid(av[1], &art)) {
	    Reply("%d No such article\r\n", NNTP_DONTHAVEIT_VAL);
	    return;
	}
	if (!PERMartok()) {
	    ARTclose();
	    Reply("%s\r\n", NOACCESS);
	    return;
	}
	tart=art;
	Reply("%d %ld %s %s\r\n", what->ReplyCode, art, what->Item, av[1]);
	if (what->Type != STstat) {
	    ARTsendmmap(what->Type);
	}
	ARTclose();
	return;
    }

    /* Trying to read. */
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }

    /* Default is to get current article, or specified article. */
    if (ac == 1) {
	if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	    Reply("%s\r\n", ARTnocurrart);
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumber);
	tart=ARTnumber;
    }
    else {
	if (strspn(av[1], "0123456789") != strlen(av[1])) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	(void)strcpy(buff, av[1]);
	tart=(ARTNUM)atol(buff);
    }

    /* Move forward until we can find one. */
    while (!ARTopen(atol(buff))) {
	if (ac > 1 || ++ARTnumber >= ARThigh) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumber);
	tart=ARTnumber;
    }
    if (ac > 1)
	ARTnumber = tart;
    if ((msgid = GetHeader("Message-ID", FALSE)) == NULL) {
        Reply("%s\r\n", ARTnoartingroup);
	return;
    }
    Reply("%d %s %.512s %s\r\n", what->ReplyCode, buff, msgid, what->Item); 
    if (what->Type != STstat) {
	ARTsendmmap(what->Type);
    }
    ARTclose();
}


/*
**  Go to the next or last (really previous) article in the group.
*/
FUNCTYPE CMDnextlast(int ac, char *av[])
{
    char	*msgid;
    int		save;
    BOOL	next;
    int		delta;
    int		errcode;
    STRING	message;

    if (!PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }
    if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	Reply("%s\r\n", ARTnocurrart);
	return;
    }

    next = (av[0][0] == 'n' || av[0][0] == 'N');
    if (next) {
	delta = 1;
	errcode = NNTP_NONEXT_VAL;
	message = "next";
    }
    else {
	delta = -1;
	errcode = NNTP_NOPREV_VAL;
	message = "previous";
    }

    save = ARTnumber;
    ARTnumber += delta;
    if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	Reply("%d No %s to retrieve.\r\n", errcode, message);
	ARTnumber = save;
	return;
    }

    while (!ARTopen(ARTnumber)) {
	ARTnumber += delta;
	if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	    Reply("%d No %s article to retrieve.\r\n", errcode, message);
	    ARTnumber = save;
	    return;
	}
    }

    if ((msgid = GetHeader("Message-ID", FALSE)) == NULL) {
        Reply("%s\r\n", ARTnoartingroup);
        return;
    }

    ARTclose();
    Reply("%d %d %s Article retrieved; request text separately.\r\n",
	   NNTP_NOTHING_FOLLOWS_VAL, ARTnumber, msgid);
}


STATIC BOOL CMDgetrange(int ac, char *av[], ARTRANGE *rp, BOOL *DidReply)
{
    char		*p;

    *DidReply = FALSE;
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	*DidReply = TRUE;
	return FALSE;
    }

    if (ac == 1) {
	/* No argument, do only current article. */
	if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	    Reply("%s\r\n", ARTnocurrart);
	    *DidReply = TRUE;
	    return FALSE;
	}
	rp->High = rp->Low = ARTnumber;
        return TRUE;
    }

    /* Got just a single number? */
    if ((p = strchr(av[1], '-')) == NULL) {
	rp->Low = rp->High = atol(av[1]);
        return TRUE;
    }

    /* Parse range. */
    *p++ = '\0';
    rp->Low = atol(av[1]);
	if (*p == '\0' || (rp->High = atol(p)) < rp->Low)
	    /* "XHDR 234-0 header" gives everything to the end. */
	rp->High = ARThigh;
    else if (rp->High > ARThigh)
	rp->High = ARThigh;
    if (rp->Low < ARTlow)
	rp->Low = ARTlow;
    p--;
    *p = '-';

    return TRUE;
}


/*
**  Return a field from the overview line or NULL on error.  Return a copy
**  since we might be re-using the line later.
*/
char *OVERGetHeader(char *p, int field)
{
    static char		*buff;
    static int		buffsize;
    int	                i, j = field;
    ARTOVERFIELD	*fp;
    char		*next, *q;
    char                *newsgroupbuff;
    BOOL                BuildingNewsgroups = FALSE;

    fp = &ARTfields[field];

    if (caseEQ(fp->Header, "Newsgroups")) {
	BuildingNewsgroups = TRUE;
	fp = &ARTfields[ARTxreffield];
    }

    /* Skip leading headers. */
    for (field; field >= 0 && *p; p++)
	if ((p = strchr(p, '\t')) == NULL)
	    return NULL;
	else
	    field--;
    if (*p == '\0')
	return NULL;

    if (fp->NeedsHeader) {		/* find an exact match */
	if (!caseEQn(fp->Header, p, fp->Length))
	    return NULL;
	p += fp->Length + 2;
	/* skip spaces */
	for (; *p && *p == ' ' ; p++);
    }

    /* Figure out length; get space. */
    if ((next = strpbrk(p, "\n\r\t")) != NULL) {
	i = next - p;
    } else {
	i = strlen(p);
    }
    if (buffsize == 0) {
	buffsize = i + VirtualPathlen;
	buff = NEW(char, buffsize + 1);
    } else if (buffsize < i + VirtualPathlen) {
	buffsize = i + VirtualPathlen;
	RENEW(buff, char, buffsize + 1);
    }

    if ((VirtualPathlen > 0) && ARTxreffield == j) {
	q = p;
	if ((q = strchr(q, ' ')) == NULL) {
	    return NULL;
	}
	memcpy(buff, VirtualPath, VirtualPathlen - 1);
	memcpy(&buff[VirtualPathlen - 1], q, next - q);
	buff[VirtualPathlen - 1 + next - q] = '\0';
    } else {
        (void)strncpy(buff, p, i);
        buff[i] = '\0';
    }

    if (BuildingNewsgroups) {
	newsgroupbuff = p = COPY(buff);
	if ((p = strchr(p, ' ')) == NULL) {
	    DISPOSE(newsgroupbuff);
	    return NULL;
	}

	for (buff[0] = '\0', q = buff, p++; *p != '\0'; ) {
	    if ((next = strchr(p, ':')) == NULL) {
		DISPOSE(newsgroupbuff);
		return NULL;
	    }
	    *next++ = '\0';
	    strcat(q, p);
	    q += (next - p - 1);
	    if ((p = strchr(next, ' ')) == NULL)
		break;
	    *p = ',';
	}
	DISPOSE(newsgroupbuff);
    }
    return buff;
}


/*
**  XOVER another extension.  Dump parts of the overview database.
*/
FUNCTYPE CMDxover(int ac, char *av[])
{
    BOOL	        DidReply;
    ARTRANGE		range;
    struct timeval	stv, etv;
    ARTNUM		artnum;
    void		*handle;
    char		*data, *p, *q;
    int			len, useIOb = 0;
    TOKEN		token;
    ARTOVERFIELD	*fp;
    int	                field;

    if (!PERMcanread) {
	Printf("%s\r\n", NOACCESS);
	return;
    }

    /* Trying to read. */
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }

    /* Parse range. */
    if (!CMDgetrange(ac, av, &range, &DidReply)) {
	if (!DidReply) {
	    Reply("%d data follows\r\n", NNTP_OVERVIEW_FOLLOWS_VAL);
	    Printf(".\r\n");
	    return;
	}
    }

    OVERcount++;
    gettimeofday(&stv, NULL);
    if ((handle = (void *)OVopensearch(GRPcur, range.Low, range.High)) == NULL) {
	if (av[1] != NULL)
	    Reply("%d %s fields follow\r\n.\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, av[1]);
	else
	    Reply("%d %d fields follow\r\n.\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, ARTnumber);
	return;
    }
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

    if (av[1] != NULL)
	Reply("%d %s fields follow\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, av[1]);
    else
	Reply("%d %d fields follow\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, ARTnumber);
    (void)fflush(stdout);
    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);

    /* If OVSTATICSEARCH is true, then the data returned by OVsearch is only
       valid until the next call to OVsearch.  In this case, we must use
       SendIOb because it copies the data. */
    OVctl(OVSTATICSEARCH, &useIOb);

    while (OVsearch(handle, &artnum, &data, &len, &token, NULL)) {
	if (PERMaccessconf->nnrpdoverstats) {
	    gettimeofday(&etv, NULL);
	    OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
	    OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
	}
	if (len == 0 || PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token)) {
	    if (PERMaccessconf->nnrpdoverstats) {
		OVERmiss++;
		gettimeofday(&stv, NULL);
	    }
	    continue;
	}
	if (PERMaccessconf->nnrpdoverstats) {
	    OVERhit++;
	    OVERsize += len;
	}
	if (VirtualPathlen > 0) {
	    /* replace path part */
	    for (field = ARTxreffield, p = data ; field-- >= 0 && p < data + len; p++)
		if ((p = strchr(p, '\t')) == NULL)
		    continue;
	    if (*p == '\0')
		continue;
	    fp = &ARTfields[ARTxreffield];
	    if (fp->NeedsHeader) {
		if (!EQn(fp->Header, p, fp->Length))
		    continue;
		p += fp->Length + 2;
		/* skip spaces */
		for (; *p && *p == ' ' ; p++);
	    }
	    if ((q = strchr(p, ' ')) == NULL)
		continue;
	    if(useIOb) {
		SendIOb(data, p - data);
		SendIOb(VirtualPath, VirtualPathlen - 1);
		SendIOb(q, len - (q - data));
	    } else {
		SendIOv(data, p - data);
		SendIOv(VirtualPath, VirtualPathlen - 1);
		SendIOv(q, len - (q - data));
	    }
	} else {
	    if(useIOb)
		SendIOb(data, len);
	    else
		SendIOv(data, len);
	}
	if (PERMaccessconf->nnrpdoverstats)
	    gettimeofday(&stv, NULL);
    }
    if (PERMaccessconf->nnrpdoverstats) {
        gettimeofday(&etv, NULL);
        OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
        OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if(useIOb) {
	SendIOb(".\r\n", 3);
	PushIOb();
    } else {
	SendIOv(".\r\n", 3);
	PushIOv();
    }
    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);
    OVclosesearch(handle);
    if (PERMaccessconf->nnrpdoverstats) {
        gettimeofday(&etv, NULL);
        OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
        OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

}


/*
**  XHDR, XPAT and PAT extensions.
*/
/* ARGSUSED */
FUNCTYPE CMDpat(int ac, char *av[])
{
    char	        *p;
    int	        	i;
    ARTRANGE		range;
    BOOL		IsLines;
    BOOL		DidReply;
    char		*header;
    char		*pattern;
    char		*text;
    int			Overview;
    ARTNUM		artnum;
    char		buff[SPOOLNAMEBUFF];
    void                *handle;
    char                *data;
    int                 len;
    TOKEN               token;

    if (!PERMcanread) {
	Printf("%s\r\n", NOACCESS);
	return;
    }

    header = av[1];
    IsLines = caseEQ(header, "lines");

    if (ac > 3)
	pattern = Glom(&av[3]);
    else
	pattern = NULL;

    do {
	/* Message-ID specified? */
	if (ac > 2 && av[2][0] == '<') {
	    p = av[2];
	    if (!ARTopenbyid(p, &artnum)) {
		Printf("%d No such article.\r\n", NNTP_DONTHAVEIT_VAL);
		return;
	    }
	    Printf("%d %s matches follow (ID)\r\n", NNTP_HEAD_FOLLOWS_VAL,
		   header);
	    if ((text = GetHeader(header, FALSE)) != NULL
		&& (!pattern || wildmat(text, pattern)))
		Printf("%s %s\r\n", p, text);

	    ARTclose();
	    Printf(".\r\n");
	    return;
	}

	if (GRPcount == 0) {
	    Reply("%s\r\n", ARTnotingroup);
	    return;
	}

	/* Range specified. */
	if (!CMDgetrange(ac - 1, av + 1, &range, &DidReply)) {
	    if (!DidReply) {
		Reply("%d %s no matches follow (range)\r\n",
		      NNTP_HEAD_FOLLOWS_VAL, header ? header : "\"\"");
		Printf(".\r\n");
		return;
	    }
	}

	/* In overview? */
	for (Overview = -1, i = 0; i < ARTfieldsize; i++)
	    if (caseEQ(ARTfields[i].Header, header)) {
		Overview = i;
		break;
	    }

	/* Not in overview, we have to fish headers out from the articles */
	if (Overview < 0 ) {
	    Reply("%d %s matches follow (art)\r\n", NNTP_HEAD_FOLLOWS_VAL,
		  header);
	    for (i = range.Low; i <= range.High && range.High > 0; i++) {
		if (!ARTopen(i))
		    continue;
		p = GetHeader(header, FALSE);
		if (p && (!pattern || wildmat(p, pattern))) {
		    sprintf(buff, "%lu ", i);
		    SendIOb(buff, strlen(buff));
		    SendIOb(p, strlen(p));
		    SendIOb("\r\n", 2);
		    ARTclose();
		}
	    }
	    SendIOb(".\r\n", 3);
	    PushIOb();
	    return;
	}

	/* Okay then, we can grab values from overview. */
	handle = (void *)OVopensearch(GRPcur, range.Low, range.High);
	if (handle == NULL) {
	    Reply("%d %s no matches follow (NOV)\r\n.\r\n",
		  NNTP_HEAD_FOLLOWS_VAL, header);
	    return;
	}	
	
	Printf("%d %s matches follow (NOV)\r\n", NNTP_HEAD_FOLLOWS_VAL,
	       header);
	while (OVsearch(handle, &artnum, &data, &len, &token, NULL)) {
	    if (len == 0 || PERMaccessconf->nnrpdcheckart
		&& !ARTinstorebytoken(token))
		continue;
	    if ((p = OVERGetHeader(data, Overview)) != NULL) {
		if (!pattern || wildmat(p, pattern)) {
		    sprintf(buff, "%lu ", artnum);
		    SendIOb(buff, strlen(buff));
		    SendIOb(p, strlen(p));
		    SendIOb("\r\n", 2);
		}
	    }
	}
	SendIOb(".\r\n", 3);
	PushIOb();
	OVclosesearch(handle);
    } while (0);

    if (pattern)
	DISPOSE(pattern);
}
