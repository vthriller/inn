/*  $Id$
**
**  Parse input to add to news overview database.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/time.h"
#include <errno.h>
#include <syslog.h>

#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "ov.h"
#include "paths.h"

unsigned int NumArts;
unsigned int StartTime;
unsigned int TotOvTime;

/*
 * Timer function (lifted from innd/timer.c). 
 * This function is designed to report the number of milliseconds since
 * the first invocation.  I wanted better resolution than time(), and
 * something easier to work with than gettimeofday()'s struct timeval's.
 */

static unsigned gettime(void)
{
    static int			init = 0;
    static struct timeval	start_tv;
    struct timeval		tv;
    
    if (! init) {
	gettimeofday(&start_tv, NULL);
	init++;
    }
    gettimeofday(&tv, NULL);
    return((tv.tv_sec - start_tv.tv_sec) * 1000 + (tv.tv_usec - start_tv.tv_usec) / 1000);
}

/*
**  Process the input.  Data comes from innd in the form:
**  @token@ data
*/

#define TEXT_TOKEN_LEN (2*sizeof(TOKEN)+2)
static void ProcessIncoming(QIOSTATE *qp)
{
    char                *Data;
    char		*p;
    TOKEN		token;
    unsigned int 	starttime, endtime;
    time_t		Time, Expires;

    for ( ; ; ) {
	/* Read the first line of data. */
	if ((Data = QIOread(qp)) == NULL) {
	    if (QIOtoolong(qp)) {
		(void)fprintf(stderr, "overchan line too long\n");
		continue;
	    }
	    break;
	}

	if (Data[0] != '@' || strlen(Data) < TEXT_TOKEN_LEN+2 
	    || Data[TEXT_TOKEN_LEN-1] != '@' || Data[TEXT_TOKEN_LEN] != ' ') {
	    fprintf(stderr, "overchan malformed token, %s\n", Data);
	    continue;
	}
	token = TextToToken(Data);
	Data += TEXT_TOKEN_LEN+1; /* skip over token and space */
	for (p = Data; !ISWHITE(*p) ;p++) ;
	*p++ = '\0';
	Time = (time_t)atol(Data);
	for (Data = p; !ISWHITE(*p) ;p++) ;
	*p++ = '\0';
	Expires = (time_t)atol(Data);
	Data = p;
	NumArts++;
	starttime = gettime();
	if (!OVadd(token, Data, strlen(Data), Time, Expires)) {
	    fprintf(stderr, "overchan: Can't write overview \"%s\", %s\n", Data, strerror(errno));
	}
	endtime = gettime();
	TotOvTime += endtime - starttime;
    }
    QIOclose(qp);
}


int main(int ac, char *av[])
{
    QIOSTATE		*qp;
    unsigned int	now;

    /* First thing, set up logging and our identity. */
    openlog("overchan", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);           
	
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    (void)umask(NEWSUMASK);
    if (innconf->enableoverview && !innconf->useoverchan)
	syslog(L_ERROR, "overchan runs while innd creates overview data(you can ignore this message if you run makehistory '-F')");

    ac -= 1;
    av += 1;

    if (!OVopen(OV_WRITE)) {
	syslog(L_FATAL, "overchan cant open overview");
	exit(1);
    }

    StartTime = gettime();
    if (ac == 0)
	ProcessIncoming(QIOfdopen(STDIN_FILENO));
    else {
	for ( ; *av; av++)
	    if (EQ(*av, "-"))
		ProcessIncoming(QIOfdopen(STDIN_FILENO));
	    else if ((qp = QIOopen(*av)) == NULL)
		(void)fprintf(stderr, "overchan cant open %s %s\n",
			*av, strerror(errno));
	    else
		ProcessIncoming(qp);
    }
    OVclose();
    now = gettime();
    syslog(L_NOTICE, "overchan timings %u arts %u of %u ms", NumArts, TotOvTime, now-StartTime);
    exit(0);
    /* NOTREACHED */
}
