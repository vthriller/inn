/*  $Id$
**
**  Expire news articles.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <syslog.h>  
#include <sys/stat.h>
#include <time.h>

#include "dbz.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"
#include "inn/history.h"


typedef struct _EXPIRECLASS {
    time_t              Keep;
    time_t              Default;
    time_t              Purge;
    bool                Missing;
    bool                ReportedMissing;
} EXPIRECLASS;

/*
**  Expire-specific stuff.
*/
#define MAGIC_TIME	49710.

static bool		EXPtracing;
static bool		EXPusepost;
static bool		Ignoreselfexpire = FALSE;
static FILE		*EXPunlinkfile;
static EXPIRECLASS      EXPclasses[NUM_STORAGE_CLASSES+1];
static const char	*EXPreason;
static time_t		EXPremember;
static time_t		Now;
static time_t		RealNow;

/* Statistics; for -v flag. */
static char		*EXPgraph;
static int		EXPverbose;
static long		EXPprocessed;
static long		EXPunlinked;
static long		EXPallgone;
static long		EXPstillhere;
static struct history	*History;
static char		*NHistory;

static void CleanupAndExit(int x);
#if ! defined (atof)            /* NEXT defines aotf as a macro */
extern double		atof();
#endif

static int EXPsplit(char *p, char sep, char **argv, int count);

enum KR {Keep, Remove};



/*
**  Open a file or give up.
*/
static FILE *
EXPfopen(bool Remove, const char *Name, char *Mode, bool Needclean)
{
    FILE *F;

    if (Remove && unlink(Name) < 0 && errno != ENOENT)
	(void)fprintf(stderr, "Warning, can't remove %s, %s\n",
		Name, strerror(errno));
    if ((F = fopen(Name, Mode)) == NULL) {
	(void)fprintf(stderr, "Can't open %s in %s mode, %s\n",
		Name, Mode, strerror(errno));
	if (Needclean)
	    CleanupAndExit(1);
	else
	    exit(1);
    }
    return F;
}


/*
**  Split a line at a specified field separator into a vector and return
**  the number of fields found, or -1 on error.
*/
static int EXPsplit(char *p, char sep, char **argv, int count)
{
    int	                i;

    if (!p)
      return 0;

    while (*p == sep)
      ++p;

    if (!*p)
      return 0;

    if (!p)
      return 0;

    while (*p == sep)
      ++p;

    if (!*p)
      return 0;

    for (i = 1, *argv++ = p; *p; )
	if (*p++ == sep) {
	    p[-1] = '\0';
	    for (; *p == sep; p++)
		;
	    if (!*p)
		return i;
	    if (++i == count)
		/* Overflow. */
		return -1;
	    *argv++ = p;
	}
    return i;
}


/*
**  Parse a number field converting it into a "when did this start?".
**  This makes the "keep it" tests fast, but inverts the logic of
**  just about everything you expect.  Print a message and return FALSE
**  on error.
*/
static bool EXPgetnum(int line, char *word, time_t *v, char *name)
{
    char	        *p;
    bool	        SawDot;
    double		d;

    if (caseEQ(word, "never")) {
	*v = (time_t)0;
	return TRUE;
    }

    /* Check the number.  We don't have strtod yet. */
    for (p = word; ISWHITE(*p); p++)
	;
    if (*p == '+' || *p == '-')
	p++;
    for (SawDot = FALSE; *p; p++)
	if (*p == '.') {
	    if (SawDot)
		break;
	    SawDot = TRUE;
	}
	else if (!CTYPE(isdigit, (int)*p))
	    break;
    if (*p) {
	(void)fprintf(stderr, "Line %d, bad `%c' character in %s field\n",
		line, *p, name);
	return FALSE;
    }
    d = atof(word);
    if (d > MAGIC_TIME)
	*v = (time_t)0;
    else
	*v = Now - (time_t)(d * 86400.);
    return TRUE;
}


/*
**  Parse the expiration control file.  Return TRUE if okay.
*/
static bool EXPreadfile(FILE *F)
{
    char	        *p;
    int	                i;
    int	                j;
    bool		SawDefault;
    char		buff[BUFSIZ];
    char		*fields[7];

    /* Scan all lines. */
    EXPremember = -1;
    SawDefault = FALSE;

    for (i = 0; i <= NUM_STORAGE_CLASSES; i++) {
	EXPclasses[i].ReportedMissing = FALSE;
        EXPclasses[i].Missing = TRUE;
    }
    
    for (i = 1; fgets(buff, sizeof buff, F) != NULL; i++) {
	if ((p = strchr(buff, '\n')) == NULL) {
	    (void)fprintf(stderr, "Line %d too long\n", i);
	    return FALSE;
	}
	*p = '\0';
        p = strchr(buff, '#');
	if (p)
	    *p = '\0';
	else
	    p = buff + strlen(buff);
	while (--p >= buff) {
	    if (isspace((int)*p))
                *p = '\0';
            else
                break;
        }
        if (buff[0] == '\0')
	    continue;
	if ((j = EXPsplit(buff, ':', fields, SIZEOF(fields))) == -1) {
	    (void)fprintf(stderr, "Line %d too many fields\n", i);
	    return FALSE;
	}

	/* Expired-article remember line? */
	if (EQ(fields[0], "/remember/")) {
	    if (j != 2) {
		(void)fprintf(stderr, "Line %d bad format\n", i);
		return FALSE;
	    }
	    if (EXPremember != -1) {
		(void)fprintf(stderr, "Line %d duplicate /remember/\n", i);
		return FALSE;
	    }
	    if (!EXPgetnum(i, fields[1], &EXPremember, "remember"))
		return FALSE;
	    continue;
	}

	/* Storage class line? */
	if (j == 4) {
            /* Is this the default line? */
            if (fields[0][0] == '*' && fields[0][1] == '\0') {
                if (SawDefault) {
                    (void)fprintf(stderr, "Line %d duplicate default\n", i);
                    return FALSE;
                }
                j = NUM_STORAGE_CLASSES;
                SawDefault = TRUE;
            } else {
                j = atoi(fields[0]);
                if ((j < 0) || (j >= NUM_STORAGE_CLASSES)) {
                    fprintf(stderr, "Line %d bad storage class %d\n", i, j);
                }
            }
	
	    if (!EXPgetnum(i, fields[1], &EXPclasses[j].Keep,    "keep")
		|| !EXPgetnum(i, fields[2], &EXPclasses[j].Default, "default")
		|| !EXPgetnum(i, fields[3], &EXPclasses[j].Purge,   "purge"))
		return FALSE;
	    /* These were turned into offsets, so the test is the opposite
	     * of what you think it should be.  If Purge isn't forever,
	     * make sure it's greater then the other two fields. */
	    if (EXPclasses[j].Purge) {
		/* Some value not forever; make sure other values are in range. */
		if (EXPclasses[j].Keep && EXPclasses[j].Keep < EXPclasses[j].Purge) {
		    (void)fprintf(stderr, "Line %d keep>purge\n", i);
		    return FALSE;
		}
		if (EXPclasses[j].Default && EXPclasses[j].Default < EXPclasses[j].Purge) {
		    (void)fprintf(stderr, "Line %d default>purge\n", i);
		    return FALSE;
		}
	    }
	    EXPclasses[j].Missing = FALSE;
	    continue;
	}

	/* Regular expiration line -- right number of fields? */
	if (j != 5) {
	    (void)fprintf(stderr, "Line %d bad format\n", i);
	    return FALSE;
	}
	continue; /* don't process this line--per-group expiry is done by expireover */
    }

    return TRUE;
}

/*
**  Should we keep the specified article?
*/
static enum KR EXPkeepit(const TOKEN *token, time_t when, time_t Expires)
{
    EXPIRECLASS         class;

    class = EXPclasses[token->class];
    if (class.Missing) {
        if (EXPclasses[NUM_STORAGE_CLASSES].Missing) {
            /* no default */
            if (!class.ReportedMissing) {
                fprintf(stderr, "Class definition %d is missing from control file, assuming never expiration\n",
                        token->class);
                EXPclasses[token->class].ReportedMissing = TRUE;
            }
            return Keep;
        } else {
            /* use the default */
            class = EXPclasses[NUM_STORAGE_CLASSES];
            EXPclasses[token->class] = class;
        }
    }
    /* Bad posting date? */
    if (when > (RealNow + 86400)) {
	/* Yes -- force the article to go to right now */
	when = Expires ? class.Purge : class.Default;
    }
    if (EXPverbose > 2) {
	if (EXPverbose > 3)
	    printf("%s age = %0.2f\n", TokenToText(*token), (Now - when) / 86400.);
	if (Expires == 0) {
	    if (when <= class.Default)
		(void)printf("%s too old (no exp)\n", TokenToText(*token));
	} else {
	    if (when <= class.Purge)
		(void)printf("%s later than purge\n", TokenToText(*token));
	    if (when >= class.Keep)
		(void)printf("%s earlier than min\n", TokenToText(*token));
	    if (Now >= Expires)
		(void)printf("%s later than header\n", TokenToText(*token));
	}
    }
    
    /* If no expiration, make sure it wasn't posted before the default. */
    if (Expires == 0) {
	if (when >= class.Default)
	    return Keep;
	
	/* Make sure it's not posted before the purge cut-off and
	 * that it's not due to expire. */
    } else {
	if (when >= class.Purge && (Expires >= Now || when >= class.Keep))
	    return Keep;
    }
    return Remove;

}


/*
**  An article can be removed.  Either print a note, or actually remove it.
**  Also fill in the article size.
*/
static void
EXPremove(const TOKEN *token)
{
    /* Turn into a filename and get the size if we need it. */
    if (EXPverbose > 1)
	(void)printf("\tunlink %s\n", TokenToText(*token));

    if (EXPtracing) {
	EXPunlinked++;
	(void)printf("%s\n", TokenToText(*token));
	return;
    }
    
    EXPunlinked++;
    if (EXPunlinkfile) {
	(void)fprintf(EXPunlinkfile, "%s\n", TokenToText(*token));
	if (!ferror(EXPunlinkfile))
	    return;
	(void)fprintf(stderr, "Can't write to -z file, %s\n",
		      strerror(errno));
	(void)fprintf(stderr, "(Will ignore it for rest of run.)\n");
	(void)fclose(EXPunlinkfile);
	EXPunlinkfile = NULL;
    }
    if (!SMcancel(*token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
	fprintf(stderr, "Can't unlink %s\n", TokenToText(*token));
}

/*
**  Do the work of expiring one line.
*/
static bool
EXPdoline(void *cookie, time_t arrived, time_t posted, time_t expires,
	  TOKEN *token)
{
    time_t		when;
    bool		HasSelfexpire = FALSE;
    bool		Selfexpired = FALSE;
    ARTHANDLE		*article;
    enum KR             kr;
    bool		r;

    if (innconf->groupbaseexpiry || SMprobe(SELFEXPIRE, token, NULL)) {
	if ((article = SMretrieve(*token, RETR_STAT)) == (ARTHANDLE *)NULL) {
	    HasSelfexpire = TRUE;
	    Selfexpired = TRUE;
	} else {
	    /* the article is still alive */
	    SMfreearticle(article);
	    if (innconf->groupbaseexpiry || !Ignoreselfexpire)
		HasSelfexpire = TRUE;
	}
    }
    if (EXPusepost && posted != 0)
	when = posted;
    else
	when = arrived;
    EXPprocessed++;
	
    if (HasSelfexpire) {
	if (Selfexpired || token->type == TOKEN_EMPTY) {
	    EXPallgone++;
	    r = false;
	} else {
	    EXPstillhere++;
	    r = true;
	}
    } else  {
	kr = EXPkeepit(token, when, expires);
	if (kr == Remove) {
	    EXPremove(token);
	    EXPallgone++;
	    r = false;
	} else {
	    EXPstillhere++;
	    r = true;
	}
    }

    return r;
}



/*
**  Clean up link with the server and exit.
*/
static void
CleanupAndExit(int x)
{
    FILE	*F;

    if (EXPunlinkfile && fclose(EXPunlinkfile) == EOF) {
	(void)fprintf(stderr, "Can't close -z file, %s\n", strerror(errno));
	x = 1;
    }

    /* Report stats. */
    if (EXPverbose) {
	(void)printf("Article lines processed %8ld\n", EXPprocessed);
	(void)printf("Articles retained       %8ld\n", EXPstillhere);
	(void)printf("Entries expired         %8ld\n", EXPallgone);
	if (!innconf->groupbaseexpiry)
	    (void)printf("Articles dropped        %8ld\n", EXPunlinked);
    }

    /* Append statistics to a summary file */
    if (EXPgraph) {
	F = EXPfopen(FALSE, EXPgraph, "a", FALSE);
	(void)fprintf(F, "%ld %ld %ld %ld %ld\n",
		      (long)Now, EXPprocessed, EXPstillhere, EXPallgone,
		      EXPunlinked);
	(void)fclose(F);
    }

    SMshutdown();
    HISclose(History);
    if (EXPreason != NULL)
	free(EXPreason);
	
    if (NHistory != NULL)
	free(NHistory);
    closelog();
    exit(x);
}

/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: expire [flags] [expire.ctl]\n");
    exit(1);
}


int main(int ac, char *av[])
{
    static char		CANTCD[] = "Can't cd to %s, %s\n";
    int                 i;
    char 	        *p;
    FILE		*F;
    char		*HistoryText;
    char		*NHistoryPath = NULL;
    char		*NHistoryText = NULL;
    char		*EXPhistdir;
    char		buff[SMBUF];
    bool		Server;
    bool		Bad;
#ifdef CANT_DO_THIS
    bool		IgnoreOld;
    bool		Writing;
#endif
    bool		UnlinkFile;
    bool		val;
    time_t		TimeWarp;

    /* First thing, set up logging and our identity. */
    openlog("expire", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Set defaults. */
    Server = TRUE;
#if CANT_DO_THIS
    IgnoreOld = FALSE;
    Writing = TRUE;
#endif
    TimeWarp = 0;
    UnlinkFile = FALSE;

    if (ReadInnConf() < 0) exit(1);

    HistoryText = concatpath(innconf->pathdb, _PATH_HISTORY);

    (void)umask(NEWSUMASK);

    /* find the default history file directory */
    EXPhistdir = COPY(HistoryText);
    p = strrchr(EXPhistdir, '/');
    if (p != NULL) {
	*p = '\0';
    }

    /* Parse JCL. */
    while ((i = getopt(ac, av, "f:h:d:g:iNnpr:tv:w:xz:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'd':
	    NHistoryPath = optarg;
	    break;
	case 'f':
	    NHistoryText = optarg;
	    break;
	case 'g':
	    EXPgraph = optarg;
	    break;
	case 'h':
	    HistoryText = optarg;
	    break;
#ifdef CANT_DO_THIS
	case 'i':
	    IgnoreOld = TRUE;
	    break;
#endif
	case 'N':
	    Ignoreselfexpire = TRUE;
	    break;
	case 'n':
	    Server = FALSE;
	    break;
	case 'p':
	    EXPusepost = TRUE;
	    break;
	case 'r':
	    EXPreason = COPY(optarg);
	    break;
	case 't':
	    EXPtracing = TRUE;
	    break;
	case 'v':
	    EXPverbose = atoi(optarg);
	    break;
	case 'w':
	    TimeWarp = (time_t)(atof(optarg) * 86400.);
	    break;
#ifdef CANT_DO_THIS
	case 'x':
	    Writing = FALSE;
	    break;
#endif
	case 'z':
	    EXPunlinkfile = EXPfopen(TRUE, optarg, "a", FALSE);
	    UnlinkFile = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;
    if ((ac != 0 && ac != 1))
	Usage();

    /* if EXPtracing is set, then pass in a path, this ensures we
     * don't replace the existing history files */
    if (EXPtracing || NHistoryText || NHistoryPath) {
	if (NHistoryPath == NULL)
	    NHistoryPath = innconf->pathdb;
	if (NHistoryText == NULL)
	    NHistoryText = _PATH_HISTORY;
	NHistory = concatpath(NHistoryPath, NHistoryText);
    }
    else {
	NHistory = NULL;
    }

    (void)time(&Now);
    RealNow = Now;
    Now += TimeWarp;

    /* Parse the control file. */
    if (av[0])
	F = EQ(av[0], "-") ? stdin : EXPfopen(FALSE, av[0], "r", FALSE);
    else {
        char *path;

        path = concatpath(innconf->pathetc, _PATH_EXPIRECTL);
	F = EXPfopen(FALSE, path, "r", FALSE);
        free(path);
    }
    if (!EXPreadfile(F)) {
	(void)fclose(F);
	(void)fprintf(stderr, "Format error in expire.ctl\n");
	exit(1);
    }
    (void)fclose(F);

    /* Set up the link, reserve the lock. */
    if (EXPreason == NULL) {
	(void)sprintf(buff, "Expiring process %ld", (long)getpid());
	EXPreason = COPY(buff);
    }

    History = HISopen(HistoryText, innconf->hismethod, HIS_RDONLY, NULL);
    if (!History) {
	fprintf(stderr, "Can't setup history manager\n");
	CleanupAndExit(1);
    }

    val = TRUE;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val)) {
	fprintf(stderr, "Can't setup storage manager\n");
	CleanupAndExit(1);
    }
    if (!SMinit()) {
	fprintf(stderr, "Can't initialize storage manager: %s\n", SMerrorstr);
	CleanupAndExit(1);
    }
    if (chdir(EXPhistdir) < 0) {
	(void)fprintf(stderr, CANTCD, EXPhistdir, strerror(errno));
	CleanupAndExit(1);
    }
    if (EXPtracing && EXPreason != NULL) {
	free(EXPreason);
	EXPreason = NULL;
    }

    Bad = HISexpire(History, NHistory, EXPreason, NULL,
		    EXPremember, EXPdoline) == false;

    if (UnlinkFile && EXPunlinkfile == NULL)
	/* Got -z but file was closed; oops. */
	Bad = TRUE;

    /* If we're done okay, and we're not tracing, slip in the new files. */
    if (EXPverbose) {
	if (Bad)
	    (void)printf("Expire errors: history files not updated.\n");
	if (EXPtracing)
	    (void)printf("Expire tracing: history files not updated.\n");
    }

    if (!Bad && NHistory != NULL) {
	(void)sprintf(buff, "%s.done", NHistory);
	(void)fclose(EXPfopen(FALSE, buff, "w", TRUE));
    }

    CleanupAndExit(Bad ? 1 : 0);
    /* NOTREACHED */
    abort();
}
