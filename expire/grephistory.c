/*  $Id$
**
**  Get data from history database.
*/

#include "clibrary.h"
#include <syslog.h>  
#include <sys/stat.h>

#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"
#include "inn/history.h"

/*
**  Read stdin for list of Message-ID's, output list of ones we
**  don't have.  Or, output list of files for ones we DO have.
*/
static void
IhaveSendme(struct history *h, char What)
{
    char		*p;
    char		*q;
    char		buff[BUFSIZ];

    while (fgets(buff, sizeof buff, stdin) != NULL) {
	time_t arrived, posted, expires;
	TOKEN token;

	for (p = buff; ISWHITE(*p); p++)
	    ;
	if (*p != '<')
	    continue;
	for (q = p; *q && *q != '>' && !ISWHITE(*q); q++)
	    ;
	if (*q != '>')
	    continue;
	*++q = '\0';

	if (!HIScheck(h, p)) {
	    if (What == 'i')
		(void)printf("%s\n", p);
	    continue;
	}

	/* Ihave -- say if we want it, and continue. */
	if (What == 'i') {
	    continue;
	}

	if (HISlookup(h, p, &arrived, &posted, &expires, &token))
	    printf("%s\n", TokenToText(token));
    }
}


/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: grephistory [flags] MessageID\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    int			i;
    const char		*History;
    char                *key;
    char		What;
    struct history	*history;
    time_t arrived, posted, expires;
    TOKEN token;


    /* First thing, set up logging and our identity. */
    openlog("grephistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    History = concatpath(innconf->pathdb, _PATH_HISTORY);

    What = '?';

    /* Parse JCL. */
    while ((i = getopt(ac, av, "f:eilnqs")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'f':
	    History = optarg;
	    break;
	case 'e':
	case 'i':
	case 'l':
	case 'n':
	case 'q':
	case 's':
	    if (What != '?') {
		(void)fprintf(stderr, "Only one [eilnqs] flag allowed.\n");
		exit(1);
	    }
	    What = (char)i;
	    break;
	}
    ac -= optind;
    av += optind;

    history = HISopen(History, innconf->hismethod, HIS_RDONLY, NULL);
    if (history == NULL) {
	(void)fprintf(stderr, "Can't open history\n");
	exit(1);
    }

    /* Set operating mode. */
    switch (What) {
    case '?':
	What = 'n';
	break;
    case 'i':
    case 's':
	IhaveSendme(history, What);
	HISclose(history);
	exit(0);
	/* NOTREACHED */
    }

    /* All modes other than -i -l want a Message-ID. */
    if (ac != 1)
	Usage();

    key = av[0];
    if (*key == '[') {
	(void)fprintf(stderr, "Accessing history by hash isn't supported\n");
	HISclose(history);
	exit(1);
    } else {
	if (*key != '<') {
	    /* Add optional braces. */
	    key = NEW(char, 1 + strlen(key) + 1 + 1);
	    (void)sprintf(key, "<%s>", key);
	}
    }

    if (!HIScheck(history, key)) {
	if (What == 'n')
	    (void)fprintf(stderr, "Not found.\n");
    }
    else if (What != 'q') {
	if (HISlookup(history, key, &arrived, &posted, &expires, &token)) {
	    if (What == 'l') {
		printf("[]\t%ld~-~%ld\t%s\n", (long)arrived, (long)posted,
		       TokenToText(token));
	    }
	    else {
		printf("%s\n", TokenToText(token));
	    }
	}
	else if (What == 'n')
	    (void)printf("/dev/null\n");
    }
    HISclose(history);
    return 0;
}
