/*  $Revision$
**
**  Read batchfiles on standard input and archive them.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include "paths.h"
#include "libinn.h"
#include "qio.h"
#include "macros.h"
#include <syslog.h> 


STATIC char	*Archive = NULL;
STATIC char	*ERRLOG = NULL;

/*
**  Return a YYYYMM string that represents the current year/month
*/
STATIC char *
DateString()
{
    STATIC char		ds[10];
    time_t		now;
    struct tm		*x;

    time(&now);
    x = localtime(&now);
    sprintf(ds, "%d%d", x->tm_year + 1900, x->tm_mon + 1);

    return ds;
}


/*
**  Try to make one directory.  Return FALSE on error.
*/
STATIC BOOL
MakeDir(Name)
    char		*Name;
{
    struct stat		Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0)
	return TRUE;

    /* See if it failed because it already exists. */
    return stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode);
}


/*
**  Given an entry, comp/foo/bar/1123, create the directory and all
**  parent directories needed.  Return FALSE on error.
*/
STATIC BOOL
MakeArchiveDirectory(Name)
    register char	*Name;
{
    register char	*p;
    register char	*save;
    BOOL		made;

    if ((save = strrchr(Name, '/')) != NULL)
	*save = '\0';

    /* Optimize common case -- parent almost always exists. */
    if (MakeDir(Name)) {
	if (save)
	    *save = '/';
	return TRUE;
    }

    /* Try to make each of comp and comp/foo in turn. */
    for (p = Name; *p; p++)
	if (*p == '/' && p != Name) {
	    *p = '\0';
	    made = MakeDir(Name);
	    *p = '/';
	    if (!made) {
		if (save)
		    *save = '/';
		return FALSE;
	    }
	}

    made = MakeDir(Name);
    if (save)
	*save = '/';
    return made;
}


/*
**  Write an index entry.  Ignore I/O errors; our caller checks for them.
*/
STATIC void
WriteIndex(FullName, ShortName)
    char		*FullName;
    char		*ShortName;
{
    static char		SUBJECT[] = "Subject:";
    static char		MESSAGEID[] = "Message-ID:";
    register char	*p;
    register QIOSTATE	*qp;
    char		Subject[BUFSIZ];
    char		MessageID[BUFSIZ];

    /* Open the file. */
    if ((qp = QIOopen(FullName)) == NULL) {
	(void)printf("%s <open error> %s\n", ShortName, strerror(errno));
	return;
    }

    /* Scan for the desired headers. */
    for (Subject[0] = '\0', MessageID[0] = '\0'; ; ) {
	if ((p = QIOread(qp)) == NULL) {
	    if (QIOerror(qp)) {
		(void)printf("%s <read error> %s\n",
			ShortName, strerror(errno));
		QIOclose(qp);
		return;
	    }
	    if (QIOtoolong(qp)) {
		(void)QIOread(qp);
		continue;
	    }

	    /* End of headers (and article) -- we're done. */
	    break;
	}

	/* End of headers -- we're done. */
	if (*p == '\0')
	    break;

	/* Is this a header we want? */
	switch (*p) {
	default:
	    continue;
	case 'S': case 's':
	    if (caseEQn(p, SUBJECT, STRLEN(SUBJECT))) {
		for (p += STRLEN(SUBJECT); ISWHITE(*p); p++)
		    continue;
		(void)strcpy(Subject, p);
	    }
	    break;
	case 'M': case 'm':
	    if (caseEQn(p, MESSAGEID, STRLEN(MESSAGEID))) {
		for (p += STRLEN(MESSAGEID); ISWHITE(*p); p++)
		    continue;
		(void)strcpy(MessageID, p);
	    }
	    break;
	}

	/* Got them all? */
	if (Subject[0] && MessageID[0])
	    break;
    }

    /* Close file, write the line. */
    QIOclose(qp);
    (void)printf("%s %s %s\n",
	    ShortName,
	    MessageID[0] ? MessageID : "<none>",
	    Subject[0] ? Subject : "<none>");
}


/*
**  Copy a file.  Return FALSE if error.
*/
STATIC BOOL
Copy(src, dest)
    char		*src;
    char		*dest;
{
    register FILE	*in;
    register FILE	*out;
    register SIZE_T	i;
    char		*p;
    char		buff[BUFSIZ];

    /* Open the output file. */
    if ((out = fopen(dest, "w")) == NULL) {
	/* Failed; make any missing directories and try again. */
	if ((p = strrchr(dest, '/')) != NULL) {
	    if (!MakeArchiveDirectory(dest)) {
		(void)fprintf(stderr, "Can't mkdir for \"%s\", %s\n",
			dest, strerror(errno));
		return FALSE;
	    }
	    out = fopen(dest, "w");
	}
	if (p == NULL || out == NULL) {
	    (void)fprintf(stderr, "Can't open \"%s\" for writing, %s\n",
		    src, strerror(errno));
	    return FALSE;
	}
    }

    /* Opening the input file is easier. */
    if ((in = fopen(src, "r")) == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\" for reading, %s\n",
		src, strerror(errno));
	(void)fclose(out);
	(void)unlink(dest);
	return FALSE;
    }

    /* Write the data. */
    while ((i = fread((POINTER)buff, (SIZE_T)1, (SIZE_T)sizeof buff, in)) != 0)
	if (fwrite((POINTER)buff, (SIZE_T)1, i, out) != i) {
	    (void)fprintf(stderr, "Can't write \"%s\", %s\n",
		    dest, strerror(errno));
	    (void)fclose(in);
	    (void)fclose(out);
	    (void)unlink(dest);
	    return FALSE;
	}
    (void)fclose(in);

    /* Flush and close the output. */
    if (ferror(out) || fflush(out) == EOF) {
	(void)fprintf(stderr, "Can't close \"%s\", %s\n",
		dest, strerror(errno));
	(void)unlink(dest);
	(void)fclose(out);
	return FALSE;
    }
    if (fclose(out) == EOF) {
	(void)fprintf(stderr, "Can't close \"%s\", %s\n",
		dest, strerror(errno));
	(void)unlink(dest);
	return FALSE;
    }

    return TRUE;
}


/*
**  Copy an article from memory into a file.
*/
STATIC BOOL
CopyArt(ARTHANDLE *art, char *dest, BOOL Concat)
{
    register FILE	*out;
    char		*p,*q,*last;
    ARTHANDLE		article;
    size_t		i;
    char		*mode = "w";

    if (Concat) mode = "a";

    /* Open the output file. */
    if ((out = fopen(dest, mode)) == NULL) {
	/* Failed; make any missing directories and try again. */
	if ((p = strrchr(dest, '/')) != NULL) {
	    if (!MakeArchiveDirectory(dest)) {
		(void)fprintf(stderr, "Can't mkdir for \"%s\", %s\n",
			dest, strerror(errno));
		return FALSE;
	    }
	    out = fopen(dest, mode);
	}
	if (p == NULL || out == NULL) {
	    (void)fprintf(stderr, "Can't open \"%s\" for writing, %s\n",
		    dest, strerror(errno));
	    return FALSE;
	}
    }

    /* Copy the data. */
    article.data = NEW(char, art->len);
    for (i=0, last=NULL, q=article.data, p=art->data; p<art->data+art->len;) {
	if (&p[1] < art->data + art->len && p[0] == '\r' && p[1] == '\n') {
	    p += 2;
	    *q++ = '\n';
	    i++;
	    if (&p[1] < art->data + art->len && p[0] == '.' && p[1] == '.') {
		p += 2;
		*q++ = '.';
		i++;
	    }
	    if (&p[2] < art->data + art->len && p[0] == '.' && p[1] == '\r' && p[2] == '\n') {
		break;
	    }
	} else {
	    *q++ = *p++;
	    i++;
	}
    }
    *q++ = '\0';

    /* Write the data. */
    if (Concat) {
	/* Write a separator... */
	fprintf(out, "-----------\n");
    }
    if (fwrite(article.data, i, 1, out) != 1) {
	(void)fprintf(stderr, "Can't write \"%s\", %s\n",
		dest, strerror(errno));
	(void)fclose(out);
	if (!Concat) (void)unlink(dest);
	DISPOSE(article.data);
	return FALSE;
    }
    DISPOSE(article.data);

    /* Flush and close the output. */
    if (ferror(out) || fflush(out) == EOF) {
	(void)fprintf(stderr, "Can't close \"%s\", %s\n",
		dest, strerror(errno));
	if (!Concat) (void)unlink(dest);
	(void)fclose(out);
	return FALSE;
    }
    if (fclose(out) == EOF) {
	(void)fprintf(stderr, "Can't close \"%s\", %s\n",
		dest, strerror(errno));
	if (!Concat) (void)unlink(dest);
	return FALSE;
    }

    return TRUE;
}


/*
**  Write an index entry.  Ignore I/O errors; our caller checks for them.
*/
STATIC void
WriteArtIndex(art, FullName, ShortName)
    ARTHANDLE		*art;
    char		*FullName;
    char		*ShortName;
{
    register char	*p;
    register int	i;
    char		Subject[BUFSIZ];
    char		MessageID[BUFSIZ];

    Subject[0] = '\0';		/* default to null string */
    p = (char *)HeaderFindMem(art->data, art->len, "Subject", 7);
    if (p != NULL) {
	for (i=0; *p != '\r' && *p != '\n' && *p != '\0'; i++) {
	    Subject[i] = *p++;
	}
	Subject[i] = '\0';
    }

    MessageID[0] = '\0';	/* default to null string */
    p = (char *)HeaderFindMem(art->data, art->len, "Message-ID", 10);
    if (p != NULL) {
	for (i=0; *p != '\r' && *p != '\n' && *p != '\0'; i++) {
	    MessageID[i] = *p++;
	}
	MessageID[i] = '\0';
    }

    (void)printf("%s %s %s\n",
	    ShortName,
	    MessageID[0] ? MessageID : "<none>",
	    Subject[0] ? Subject : "<none>");
}


/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage()
{
    (void)fprintf(stderr, "Usage error.\n");
    exit(1);
}


/*
** Crack an Xref line apart into separate strings, each of the form "ng:artnum".
** Return in "lenp" the number of newsgroups found.
** 
** This routine blatantly stolen from tradspool.c
*/
static char **
CrackXref(char *xref, unsigned int *lenp) {
    char *p;
    char **xrefs;
    char *q;
    unsigned int len, xrefsize;
    unsigned int slen;

    len = 0;
    xrefsize = 5;
    xrefs = NEW(char *, xrefsize);

    /* skip pathhost */
    if ((p = strchr(xref, ' ')) == NULL) {
	fprintf(stderr, "archive: Could not find pathhost in Xref header");
	return NULL;
    }
    /* skip next spaces */
    for (p++; *p == ' ' ; p++) ;
    while (TRUE) {
	/* check for EOL */
	/* shouldn't ever hit null w/o hitting a \r\n first, but best to be paranoid */
	if (*p == '\n' || *p == '\r' || *p == 0) {
	    /* hit EOL, return. */
	    *lenp = len;
	    return xrefs;
	}
	/* skip to next space or EOL */
	for (q=p; *q && *q != ' ' && *q != '\n' && *q != '\r' ; ++q) ;

	slen = q-p;
	xrefs[len] = NEW(char, slen+1);
	strncpy(xrefs[len], p, slen);
	xrefs[len][slen] = '\0';

	if (++len == xrefsize) {
	    /* grow xrefs if needed. */
	    xrefsize *= 2;
	    RENEW(xrefs, char *, xrefsize);
	}

 	p = q;
	/* skip spaces */
	for ( ; *p == ' ' ; p++) ;
    }
}


/*
** Crack an groups pattern parameter apart into separate strings
** Return in "lenp" the number of patterns found.
*/
static char **
CrackGroups(char *group, unsigned int *lenp) {
    char *p;
    char **groups;
    char *q;
    unsigned int len, grpsize;
    unsigned int slen;

    len = 0;
    grpsize = 5;
    groups = NEW(char *, grpsize);

    /* skip leading spaces */
    for (p=group; *p == ' ' ; p++) ;
    while (TRUE) {
	/* check for EOL */
	/* shouldn't ever hit null w/o hitting a \r\n first, but best to be paranoid */
	if (*p == '\n' || *p == '\r' || *p == 0) {
	    /* hit EOL, return. */
	    *lenp = len;
	    return groups;
	}
	/* skip to next comma, space, or EOL */
	for (q=p; *q && *q != ',' && *q != ' ' && *q != '\n' && *q != '\r' ; ++q) ;

	slen = q-p;
	groups[len] = NEW(char, slen+1);
	strncpy(groups[len], p, slen);
	groups[len][slen] = '\0';

	if (++len == grpsize) {
	    /* grow groups if needed. */
	    grpsize *= 2;
	    RENEW(groups, char *, grpsize);
	}

 	p = q;
	/* skip commas and spaces */
	for ( ; *p == ' ' || *p == ',' ; p++) ;
    }
}


int
main(ac, av)
    int			ac;
    char		*av[];
{
    register char	*Name;
    register char	*p;
    register FILE	*F;
    register int	i;
    BOOL		Flat;
    BOOL		Move;
    BOOL		Redirect;
    BOOL		Concat;
    char		*Index;
    char		*last;
    char		buff[BUFSIZ];
    char		temp[BUFSIZ];
    char		dest[BUFSIZ];
    char		**groups, *q, *ng;
    char		**xrefs;
    char		*xrefhdr;
    ARTHANDLE		*art;
    TOKEN		token;
    struct stat		Sb;
    unsigned int	numgroups, numxrefs;
    int			j;
    char		*base = NULL;
    BOOL		doit;

    /* First thing, set up logging and our identity. */
    openlog("archive", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    Concat = FALSE;
    Flat = FALSE;
    Index = NULL;
    Move = FALSE;
    Redirect = TRUE;
    (void)umask(NEWSUMASK);
    ERRLOG = COPY(cpcatpath(innconf->pathlog, _PATH_ERRLOG));
    Archive = innconf->patharchive;
    groups = NULL;
    numgroups = 0;

    /* Parse JCL. */
    while ((i = getopt(ac, av, "a:cfi:mp:r")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'a':
	    Archive = optarg;
	    break;
	case 'c':
	    Flat = TRUE;
	    Concat = TRUE;
	    break;
	case 'f':
	    Flat = TRUE;
	    break;
	case 'i':
	    Index = optarg;
	    break;
	case 'm':
	    Move = TRUE;
	    break;
	case 'p':
	    groups = CrackGroups(optarg, &numgroups);
	    break;
	case 'r':
	    Redirect = FALSE;
	    break;
	}
#if !defined(HAVE_SYMLINK)
	if (Move)
	    (void)fprintf(stderr, "archive:  Ignoring ``-m'' flag\n");
#endif	/* !defined(HAVE_SYMLINK) */

    /* Parse arguments -- at most one, the batchfile. */
    ac -= optind;
    av += optind;
    if (ac > 2)
	Usage();

    /* Do file redirections. */
    if (Redirect)
	(void)freopen(ERRLOG, "a", stderr);
    if (ac == 1 && freopen(av[0], "r", stdin) == NULL) {
	(void)fprintf(stderr, "archive:  Can't open \"%s\" for input, %s\n",
		av[0], strerror(errno));
	    exit(1);
    }
    if (Index && freopen(Index, "a", stdout) == NULL) {
	(void)fprintf(stderr, "archive:  Can't open \"%s\" for output, %s\n",
		Index, strerror(errno));
	exit(1);
    }

    /* Go to where the action is. */
    if (chdir(innconf->patharticles) < 0) {
	(void)fprintf(stderr, "archive:  Can't cd to \"%s\", %s\n",
		innconf->patharticles, strerror(errno));
	exit(1);
    }

    /* Set up the destination. */
    (void)strcpy(dest, Archive);
    Name = dest + strlen(dest);
    *Name++ = '/';

    /* If storageapi is being used, initialize... */
    if (innconf->storageapi) {
	if (!SMinit()) {
	    (void)fprintf(stderr, "archive: Could not initialize the storage manager: %s", SMerrorstr);
	    exit(1);
	}
    }

    /* Read input. */
    while (fgets(buff, sizeof buff, stdin) != NULL) {
	if ((p = strchr(buff, '\n')) == NULL) {
	    (void)fprintf(stderr,
		    "archive:  Skipping \"%.40s...\" -- too long\n",
		    buff);
	    continue;
	}
	*p = '\0';
	if (buff[0] == '\0' || buff[0] == COMMENT_CHAR)
	    continue;

	/* Check to see if this is a token... */
	if (IsToken(buff)) {
	    /* Get a copy of the article. */
	    token = TextToToken(buff);
	    if ((art = SMretrieve(token, RETR_ALL)) == NULL) {
		(void)fprintf(stderr, "Could not retrieve %s\n",
			buff);
		continue;
	    }

	    /* Determine groups from the Xref header */
    	    xrefhdr = (char *)HeaderFindMem(art->data, art->len, "Xref", 4);
	    if (xrefhdr == NULL) {
		fprintf(stderr, "archive: cant find Xref: header");
		SMfreearticle(art);
		continue;
	    }

	    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
		fprintf(stderr, "archive: bogus Xref: header");
		SMfreearticle(art);
		continue;
	    }

	    /* Process each newsgroup... */
	    if (base) {
		DISPOSE(base);
		base = NULL;
	    }
	    for (i=0; i<numxrefs; i++) {
		/* Check for group limits... -p flag */
		if ((p=strchr(xrefs[i], ':')) == NULL) {
		    fprintf(stderr, "archive: bogus xref '%s'\n", xrefs[i]);
		    continue;	/* Skip to next xref */
		}
		if (numgroups > 0) {
		    *p = '\0';
		    ng = xrefs[i];
		    doit = FALSE;
		    for (j=0; j<numgroups && !doit; j++) {
			if (wildmat(ng, groups[j]) != 0) doit=TRUE;
		    }
		}
		else {
		    doit = TRUE;
		}
		*p = '/';
		if (doit) {
		    p = Name;
		    q = xrefs[i];
		    while(*q) {
		        *p++ = *q++;
		    }
		    *p='\0';

		    if (!Flat) {
		        for (p=Name; *p; p++) {
			    if (*p == '.') {
			        *p = '/';
			    }
		        }
		    }

		    if (Concat) {
			p = strrchr(Name, '/');
			q = DateString();
			p++;
			while (*q) {
			    *p++ = *q++;
			}
			*p = '\0';
		    }
			
		    if (base) {
			/* Try to link the file into the archive. */
			if (link(base, dest) < 0) {

			    /* Make the archive directory. */
			    if (!MakeArchiveDirectory(dest)) {
				(void)fprintf(stderr, "Can't mkdir for \"%s\", %s\n",
					dest, strerror(errno));
				continue;
			    }

			    /* Try to link again; if that fails, make a copy. */
			    if (link(base, dest) < 0) {
#if	defined(HAVE_SYMLINK)
				if (symlink(base, dest) < 0)
				    (void)fprintf(stderr, "Can't symlink \"%s\" to \"%s\", %s\n",
					    dest, base, strerror(errno));
				else
#endif	/* defined(HAVE_SYMLINK) */
				if (!Copy(base, dest))
				    continue;
				continue;
			    }
			}
		    } else {
			if (!CopyArt(art, dest, Concat)) {
			    fprintf(stderr, 
				"archive: %s->%s failed\n", buff, dest);
			}
			base = COPY(dest);
		    }

	            /* Write index. */
	            if (Index) {
	                WriteArtIndex(art, dest, Name);
	                if (ferror(stdout) || fflush(stdout) == EOF)
		            (void)fprintf(stderr, "Can't write index for \"%s\", %s\n",
			            Name, strerror(errno));
	            }
		}
	    }

	    /* Free up the article storage space */
	    SMfreearticle(art);
	    art = NULL;
	    /* Free up the xrefs storage space */
	    for ( i=0; i<numxrefs; i++) DISPOSE(xrefs[i]);
	    DISPOSE(xrefs);
	    numxrefs = 0;
	    xrefs = NULL;
	}
	else {
	    /* Make sure we're only copying files. */
	    if (stat(buff, &Sb) < 0) {
	        if (errno != ENOENT)
		    (void)fprintf(stderr, "Can't stat \"%s\", %s\n",
			    buff, strerror(errno));
	        continue;
	    }
	    if (!S_ISREG(Sb.st_mode)) {
	        (void)fprintf(stderr, "\"%s\" is not a regular file\n", buff);
	        continue;
	    }

	    /* Set up the destination name. */
	    (void)strcpy(Name, buff);
	    if (Flat) {
	        for (last = NULL, p = Name; *p; p++)
		    if (*p == '/') {
		        last = p;
		        *p = '.';
		    }
	        if (last)
		    *last = '/';
	    }

#if	defined(HAVE_SYMLINK)
	    if (Move) {
	        if (!Copy(buff, dest))
		    continue;
	        if (unlink(buff) < 0 && errno != ENOENT)
		    (void)fprintf(stderr, "Can't remove \"%s\", %s\n",
			    buff, strerror(errno));
	        if (symlink(dest, buff) < 0)
		    (void)fprintf(stderr, "Can't symlink \"%s\" to \"%s\", %s\n",
			    buff, dest, strerror(errno));
	        continue;
	    }
#endif	/* defined(HAVE_SYMLINK) */

	    /* Try to link the file into the archive. */
	    if (link(buff, dest) < 0) {

	        /* Make the archive directory. */
	        if (!MakeArchiveDirectory(dest)) {
		    (void)fprintf(stderr, "Can't mkdir for \"%s\", %s\n",
			    dest, strerror(errno));
		    continue;
	        }

	        /* Try to link again; if that fails, make a copy. */
	        if (link(buff, dest) < 0 && !Copy(buff, dest))
		    continue;
	    }

	    /* Write index. */
	    if (Index) {
	        WriteIndex(dest, Name);
	        if (ferror(stdout) || fflush(stdout) == EOF)
		    (void)fprintf(stderr, "Can't write index for \"%s\", %s\n",
			    Name, strerror(errno));
	    }
	}
    }

    /* close down the storage manager api */
    if (innconf->storageapi) {
	SMshutdown();
    }

    /* If we read all our input, try to remove the file, and we're done. */
    if (feof(stdin)) {
	(void)fclose(stdin);
	if (av[0])
	    (void)unlink(av[0]);
	exit(0);
    }

    /* Make an appropriate spool file. */
    p = av[0];
    if (p == NULL)
	(void)sprintf(temp, "%s/%s", innconf->pathoutgoing, "archive");
    else if (*p == '/')
	(void)sprintf(temp, "%s.bch", p);
    else
	(void)sprintf(temp, "%s/%s.bch", innconf->pathoutgoing, p);
    if ((F = xfopena(temp)) == NULL) {
	(void)fprintf(stderr, "archive: Can't spool to \"%s\", %s\n",
	    temp, strerror(errno));
	exit(1);
    }

    /* Write the rest of stdin to the spool file. */
    i = 0;
    if (fprintf(F, "%s\n", buff) == EOF) {
	(void)fprintf(stderr, "archive:  Can't start spool, %s\n",
		strerror(errno));
	i = 1;
    }
    while (fgets(buff, sizeof buff, stdin) != NULL) 
	if (fputs(buff, F) == EOF) {
	    (void)fprintf(stderr, "archive: Can't write spool, %s\n",
		    strerror(errno));
	    i = 1;
	    break;
	}
    if (fclose(F) == EOF) {
	(void)fprintf(stderr, "archive: Can't close spool, %s\n",
	    strerror(errno));
	i = 1;
    }

    /* If we had a named input file, try to rename the spool. */
    if (p != NULL && rename(temp, av[0]) < 0) {
	(void)fprintf(stderr, "archive: Can't rename spool, %s\n",
	    strerror(errno));
	i = 1;
    }

    exit(i);
    /* NOTREACHED */
}
