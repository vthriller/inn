/*  $Revision$
**
**  Net News Reading Protocol server.
*/
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <sys/file.h>
#if	defined(VAR_VARARGS)
#include <varargs.h>
#endif	/* defined(VAR_VARARGS) */
#if	defined(VAR_STDARGS)
#include <stdarg.h>
#endif	/* defined(VAR_STDARGS) */
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include <syslog.h> 
#include "paths.h"
#include "nntp.h"
#include "libinn.h"
#include "qio.h"
#include "macros.h"


/*
**  Maximum input line length, sigh.
*/
#define ART_LINE_LENGTH		1000
#define ART_LINE_MALLOC		1024
#define ART_MAX			1024


/*
**  Some convenient shorthands.
*/
typedef struct in_addr	INADDR;
#define Printf		printf
#if	defined(VAR_NONE)
#define Reply		printf
#endif	/* defined(VAR_NONE) */


/*
**  A range of article numbers.
*/
typedef struct _ARTRANGE {
    int		Low;
    int		High;
} ARTRANGE;


/*
**  What READline returns.
*/
typedef enum _READTYPE {
    RTeof,
    RTok,
    RTlong,
    RTtimeout
} READTYPE;

/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char	*Header;
    int		Length;
    BOOL	HasHeader;
    BOOL	NeedsHeader;
} ARTOVERFIELD;

#if	defined(MAINLINE)
#define EXTERN	/* NULL */
#else
#define EXTERN	extern
#endif	/* defined(MAINLINE) */

EXTERN BOOL	PERMauthorized;
EXTERN BOOL	PERMcanpost;
EXTERN BOOL	PERMcanread;
EXTERN BOOL	PERMneedauth;
EXTERN BOOL	PERMspecified;
EXTERN BOOL	PERMnewnews;
EXTERN BOOL	PERMlocpost;
EXTERN BOOL	Tracing;
EXTERN BOOL 	Offlinepost;
EXTERN char	**PERMreadlist;
EXTERN char	**PERMpostlist;
EXTERN char	ClientHost[SMBUF];
EXTERN char     ServerHost[SMBUF];
EXTERN char	Username[SMBUF];
EXTERN char     ClientIp[20];
EXTERN char	LogName[256] ;
extern char	*ACTIVETIMES;
extern char	*HISTORY;
extern char	*ACTIVE;
extern char	*NEWSGROUPS;
extern char	*NNRPACCESS;
extern char	NOACCESS[];
EXTERN ARTOVERFIELD	*ARTfields;
EXTERN int	ARTfieldsize;
EXTERN int	SPOOLlen;
EXTERN char	PERMpass[SMBUF];
EXTERN char	PERMuser[SMBUF];
EXTERN FILE	*locallog;
EXTERN int	ARTnumber;	/* Current article number */
EXTERN int	ARThigh;	/* Current high number for group */
EXTERN int	ARTlow;		/* Current low number for group */
EXTERN long	ARTcount;	/* Current number of articles in group */
EXTERN long	MaxBytesPerSecond; /* maximum bytes per sec a client can use, defaults to 0 */
EXTERN long	ARTget;
EXTERN long	ARTgettime;
EXTERN long	ARTgetsize;
EXTERN long	OVERcount;	/* number of XOVER commands			*/
EXTERN long	OVERhit;	/* number of XOVER records found in .overview	*/
EXTERN long	OVERmiss;	/* number of XOVER records found in articles	*/
EXTERN long	OVERtime;	/* number of ms spent sending XOVER data	*/
EXTERN long	OVERsize;	/* number of bytes of XOVER data sent		*/
EXTERN long	OVERdbz;	/* number of ms spent reading dbz data	*/
EXTERN long	OVERseek;	/* number of ms spent seeking history	*/
EXTERN long	OVERget;	/* number of ms spent reading history	*/
EXTERN long	OVERartcheck;	/* number of ms spent article check	*/
EXTERN long	GRParticles;
EXTERN long	GRPcount;
EXTERN char	*GRPcur;
EXTERN long	POSTreceived;
EXTERN long	POSTrejected;

EXTERN BOOL     BACKOFFenabled;
EXTERN long     ClientIP;                                 


#if	NNRP_LOADLIMIT > 0
extern int		GetLoadAverage();
#endif	/* NNRP_LOADLIMIT > 0 */
extern STRING		ARTpost();
extern void		ARTclose();
extern void		ARTreadschema();
extern char		*Glom();
extern int		Argify();
extern NORETURN		ExitWithStats();
extern BOOL		GetGroupList();
extern char		*GetHeader();
extern void		GRPreport();
extern void		HIScheck();
extern char		*HISgetent();
extern long		LOCALtoGMT();
extern BOOL		NGgetlist();
extern long		NNTPtoGMT();
extern BOOL		PERMartok();
extern BOOL		PERMmatch();
extern BOOL		ParseDistlist();
extern READTYPE		READline();
extern char		*OVERGetHeader(char *p, int field);
#if	defined(VAR_STDARGS)
extern void		Reply(char *, ...);
#endif	/* defined(VAR_STDARGS) */
#if	defined(VAR_VARARGS)
extern void		Reply();
#endif	/* defined(VAR_VARARGS) */

char *HandleHeaders(char *article);
int perlConnect(char *ClientHost, char *ClientIP, char *ServerHost, char *accesslist);
int perlAuthenticate(char *ClientHost, char *ClientIP, char *ServerHost, char *user, char *passwd, char *accesslist);
BOOL ARTinstorebytoken(TOKEN token);

