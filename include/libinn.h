/*  $Id$
**
**  Here be declarations of functions in the InterNetNews library.
*/

#ifndef LIBINN_H
#define LIBINN_H 1

#include "inn/defines.h"

/* Eventually we don't want to install this, since this will be an installed
   header and we don't want to install config.h. */
#include "config.h"

#include <stdarg.h>             /* va_list */
#include <stdio.h>              /* FILE */
#include <sys/types.h>          /* size_t and ssize_t */

/* Forward declarations to avoid unnecessary includes. */
struct stat;
struct iovec;

BEGIN_DECLS

/*
**  ERROR HANDLING AND LOGGING
*/
extern void warn(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void syswarn(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void die(const char *, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));
extern void sysdie(const char *, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));
extern void warn_set_handlers(int count, ...);
extern void die_set_handlers(int count, ...);

/* Handlers intended to be passed to *_set_handlers.  error_log_stderr is
   the only handler enabled by default. */
extern void error_log_stderr(int, const char *, va_list, int);
extern void error_log_syslog_crit(int, const char *, va_list, int);
extern void error_log_syslog_err(int, const char *, va_list, int);
extern void error_log_syslog_warning(int, const char *, va_list, int);

/* Log handling functions take the length of the resulting message, the
   format, the arguments, and the errno if any. */
typedef void (*error_handler_t)(int, const char *, va_list, int);

/* If non-NULL, called before exit and its return value passed to exit. */
extern int (*error_fatal_cleanup)(void);

/* If non-NULL, prepended (followed by ": ") to all error messages. */
extern const char *error_program_name;


/*
**  MEMORY MANAGEMENT
*/

/* The xmalloc, xrealloc, and xstrdup functions are actually macros so that
   we can pick up the file and line number information for debugging error
   messages without the user having to pass those in every time. */
#define xmalloc(size)           x_malloc((size), __FILE__, __LINE__)
#define xrealloc(p, size)       x_realloc((p), (size), __FILE__, __LINE__)
#define xstrdup(p)              x_strdup((p), __FILE__, __LINE__)

/* Last two arguments are always file and line number.  These are internal
   implementations that should not be called directly.  ISO C99 says that
   identifiers beginning with _ and a lowercase letter are reserved for
   identifiers of file scope, so while the position of libraries in the
   standard isn't clear, it's probably not entirely kosher to use _xmalloc
   here.  Use x_malloc instead. */
extern void *x_malloc(size_t, const char *, int);
extern void *x_realloc(void *, size_t, const char *, int);
extern char *x_strdup(const char *, const char *, int);

/* Failure handler takes the function, the size, the file, and the line. */
typedef void (*xmalloc_handler_t)(const char *, size_t, const char *, int);

/* The default error handler. */
void xmalloc_fail(const char *, size_t, const char *, int);

/* Assign to this variable to choose a handler other than the default, which
   just calls sysdie. */
extern xmalloc_handler_t xmalloc_error_handler;


/*
**  TIME AND DATE PARSING, GENERATION, AND HANDLING
*/
typedef struct _TIMEINFO {
    time_t      time;
    long        usec;
    long        tzone;
} TIMEINFO;

extern int      GetTimeInfo(TIMEINFO *Now);
extern bool     makedate(time_t, bool local, char *buff, size_t buflen);
extern time_t   parsedate(char *p, TIMEINFO *now);
extern time_t   parsedate_nntp(const char *, const char *, bool local);


/*
**  VERSION INFORMATION
*/
extern const int        inn_version[3];
extern const char       inn_version_extra[];
extern const char       inn_version_string[];

/* Earlier versions of INN didn't have <inn/version.h> and some source is
   intended to be portable to different INN versions; it can use this macro
   to determine whether <inn/version.h> is available. */
#define HAVE_INN_VERSION_H 1


/*
**  WILDMAT MATCHING
*/
enum wildmat {
    WILDMAT_FAIL   = 0,
    WILDMAT_MATCH  = 1,
    WILDMAT_POISON
};

extern bool             wildmat(const char *text, const char *pat);
extern enum wildmat     wildmat_poison(const char *text, const char *pat);


/*
**  FILE LOCKING
*/
enum locktype {
    LOCK_READ,
    LOCK_WRITE,
    LOCK_UNLOCK
};

extern bool     lock_file(int fd, enum locktype type, bool block);
extern bool     lock_range(int fd, enum locktype type, bool block,
                           off_t offset, off_t size);


/*
**  MISCELLANEOUS UTILITY FUNCTIONS
*/
extern void     close_on_exec(int fd, bool flag);
extern char *   concat(const char *first, ...);
extern char *   concatpath(const char *base, const char *name);
extern void     daemonize(const char *path);
extern int      getfdlimit(void);
extern int      nonblocking(int fd, bool flag);
extern int      setfdlimit(int limit);
extern void     (*xsignal(int signum, void (*sigfunc)(int)))(int);
extern ssize_t  xwrite(int fd, const void *buffer, size_t size);
extern ssize_t  xwritev(int fd, const struct iovec *iov, int iovcnt);


/* Headers. */
extern char *           GenerateMessageID(char *domain);
extern const char *     HeaderFindMem(const char *Article, int ArtLen,
                                      const char *Header, int HeaderLen); 
extern const char *     HeaderFindDisk(const char *file, const char *Header,
                                       int HeaderLen);
extern void             HeaderCleanFrom(char *from);
extern struct _DDHANDLE * DDstart(FILE *FromServer, FILE *ToServer);
extern void               DDcheck(struct _DDHANDLE *h, char *group);
extern char *             DDend(struct _DDHANDLE *h);

/* NNTP functions. */
extern int      NNTPlocalopen(FILE **FromServerp, FILE **ToServerp,
                              char *errbuff);
extern int      NNTPremoteopen(int port, FILE **FromServerp,
                               FILE **ToServerp, char *errbuff);
extern int      NNTPconnect(char *host, int port, FILE **FromServerp,
                            FILE **ToServerp, char *errbuff);
extern int      NNTPsendarticle(char *, FILE *F, bool Terminate);
extern int      NNTPsendpassword(char *server, FILE *FromServer,
                                 FILE *ToServer);
extern int      server_init(char *host, int port);

/* Opening the active file on a client. */
extern FILE *   CAopen(FILE *FromServer, FILE *ToServer);
extern FILE *   CAlistopen(FILE *FromServer, FILE *ToServer,
			   const char *request);
extern FILE *   CA_listopen(char *pathname, FILE *FromServer, FILE *ToServer,
			    const char *request);
extern void     CAclose(void);

    
/*
**  INNCONF SETTINGS
**
**  This structure is organized in the same order as the variables contained
**  in it are mentioned in the inn.conf documentation, and broken down into
**  the same sections.
*/
struct conf_vars {
    /* General Settings */
    char *domain;               /* Default domain of local host */
    char *mailcmd;              /* Command to send report/control type mail */
    char *mta;                  /* MTA for mailing to moderators, innmail */
    char *pathhost;             /* Entry for the Path line */
    char *server;               /* Default server to connect to */

    /* Feed Configuration */
    int artcutoff;              /* Max accepted article age */
    char *bindaddress;          /* Which interface IP to bind to */
    int hiscachesize;           /* Size of the history cache in kB */
    int ignorenewsgroups;       /* Propagate cmsgs by affected group? */
    int immediatecancel;        /* Immediately cancel timecaf messages? */
    int linecountfuzz;          /* Check linecount and reject if off by more */
    long maxartsize;            /* Reject articles bigger than this */
    int maxconnections;         /* Max number of incoming NNTP connections */
    char *pathalias;            /* Prepended Host for the Path line */
    int port;                   /* Which port innd should listen on */
    int refusecybercancels;     /* Reject message IDs with "<cancel."? */
    int remembertrash;          /* Put unwanted article IDs into history */
    char *sourceaddress;        /* Source IP for outgoing NNTP connections */
    int usecontrolchan;         /* Use a channel feed for control messages? */
    int verifycancels;          /* Verify cancels against article author */
    int wanttrash;              /* Put unwanted articles in junk */
    int wipcheck;               /* How long to defer other copies of article */
    int wipexpire;              /* How long to keep pending article record */
    int dontrejectfiltered;     /* Don't reject filtered article? */

    /* Article Storage */
    long cnfscheckfudgesize;    /* Additional CNFS integrity checking */
    int enableoverview;         /* Store overview info for articles? */
    int groupbaseexpiry;        /* Do expiry by newsgroup? */
    int mergetogroups;          /* Refile articles from to.* into to */
    int overcachesize;          /* fd size cache for tradindexed */
    char *ovgrouppat;           /* Newsgroups to store overview for */
    char *ovmethod;             /* Which overview method to use */
    int storeonxref;            /* SMstore use Xref to detemine class? */
    int useoverchan;            /* overchan write the overview, not innd? */
    int wireformat;             /* Store tradspool artilces in wire format? */
    int xrefslave;              /* Act as a slave of another server? */

    /* Reading */
    int allownewnews;           /* Allow use of the NEWNEWS command */
    int articlemmap;            /* Use mmap to read articles? */
    int clienttimeout;          /* How long nnrpd can be inactive */
    int nnrpdcheckart;          /* Check article existence before returning? */
    int nnrpperlauth;           /* Use Perl for nnrpd authentication */
    int nnrppythonauth;         /* Use Python for nnrpd authentication */
    int noreader;               /* Refuse to fork nnrpd for readers? */
    int readerswhenstopped;     /* Allow nnrpd when server is paused */
    int readertrack;            /* Use the reader tracking system? */

    /* Reading -- Keyword Support */
    char keywords;              /* Generate keywords in overview? */
    int keyartlimit;            /* Max article size for keyword generation */
    int keylimit;               /* Max allocated space for keywords */
    int keymaxwords;            /* Max count of interesting works */

    /* Posting */
    int addnntppostingdate;     /* Add NNTP-Posting-Date: to posts */
    int addnntppostinghost;     /* Add NNTP-Posting-Host: to posts */
    int checkincludedtext;      /* Reject if too much included text */
    char *complaints;           /* Address for X-Complaints-To: */
    char *fromhost;             /* Host for the From: line */
    long localmaxartsize;       /* Max article size of local postings */
    char *moderatormailer;      /* Default host to mail moderated articles */
    int nnrpdauthsender;        /* Add authenticated Sender: header? */
    char *nnrpdposthost;        /* Host postings should be forwarded to */
    int nnrpdpostport;          /* Port postings should be forwarded to */
    char *organization;         /* Data for the Organization: header */
    int spoolfirst;             /* Spool all posted articles? */
    int strippostcc;            /* Strip To:, Cc: and Bcc: from posts */

    /* Posting -- Exponential Backoff */
    int backoff_auth;           /* Backoff by user, not IP address */
    char *backoff_db;           /* Directory for backoff databases */
    long backoff_k;             /* Multiple for the sleep time */
    long backoff_postfast;      /* Upper time limit for fast posting */
    long backoff_postslow;      /* Lower time limit for slow posting */
    long backoff_trigger;       /* Number of postings before triggered */

    /* Logging */
    int logartsize;             /* Log article sizes? */
    int logcancelcomm;          /* Log ctlinnd cancel commands to syslog? */
    int logipaddr;              /* Log by host IP address? */
    int logsitename;            /* Log outgoing site names? */
    int nnrpdoverstats;         /* Log overview statistics? */
    int nntpactsync;            /* Checkpoint log after this many articles */
    int nntplinklog;            /* Put storage token into the log? */
    int status;                 /* Status file update interval */
    int timer;                  /* Performance monitoring interval */

    /* System Tuning */
    int badiocount;             /* Failure count before dropping channel */
    int blockbackoff;           /* Multiplier for sleep in EAGAIN writes */
    int chaninacttime;          /* Wait before noticing inactive channels */
    int chanretrytime;          /* How long before channel restarts */
    int icdsynccount;           /* Articles between active & history updates */
    int maxforks;               /* Give up after this many fork failure */
    int nicekids;               /* Child processes get niced to this */
    int nicenewnews;            /* If NEWNEWS command is used, nice to this */
    int nicennrpd;              /* nnrpd is niced to this */
    int pauseretrytime;         /* Seconds before seeing if pause is ended */
    int peertimeout;            /* How long peers can be inactive */
    int rlimitnofile;           /* File descriptor limit to set */
    int keepmmappedthreshold;  /* threshold for keep mmapped for buffindexed */

    /* Paths */
    char *patharchive;          /* Archived news. */
    char *patharticles;         /* Articles. */
    char *pathbin;              /* News binaries. */
    char *pathcontrol;          /* Control message processing routines */
    char *pathdb;               /* News database files */
    char *pathetc;              /* News configuration files */
    char *pathfilter;           /* Filtering code */
    char *pathhttp;             /* HTML files */
    char *pathincoming;         /* Incoming spooled news */
    char *pathlog;              /* Log files */
    char *pathnews;             /* Home directory for news user */
    char *pathoutgoing;         /* Outgoing news batch files */
    char *pathoverview;         /* Overview infomation */
    char *pathrun;              /* Runtime state and sockets */
    char *pathspool;            /* Root of news spool hierarchy */
    char *pathtmp;              /* Temporary files for the news system */
};

extern struct conf_vars *innconf;
extern const char *innconffile;
extern char *    GetFQDN(char *domain);
extern char *    GetConfigValue(char *value);
extern char *    GetFileConfigValue(char *value);
extern bool      GetBooleanConfigValue(char *value, bool DefaultValue);
extern char *    GetModeratorAddress(FILE *FromServer, FILE *ToServer,
                                     char *group, char *moderatormailer); 
extern void  ClearInnConf(void);
extern int ReadInnConf(void);
extern const char *cpcatpath(const char *p, const char *f);

#define TEMPORARYOPEN   0
#define INND_HISTORY    1
#define DBZ_DIR         2
#define DBZ_BASE        3

/* Hash functions */
typedef struct {
    char        hash[16];
} HASH;
extern HASH     Hash(const void *value, const size_t len);
/* Return the hash of a case mapped message-id */
extern HASH     HashMessageID(const char *MessageID);
extern bool     HashEmpty(const HASH hash);
extern void     HashClear(HASH *hash);
extern char *   HashToText(const HASH hash);
extern HASH     TextToHash(const char *text);
extern int      HashCompare(const HASH *h1, const HASH *h2);

/* Miscellaneous. */
extern int      dbzneedfilecount(void);
extern bool     MakeDirectory(char *Name, bool Recurse);
extern int      xread(int fd, char *p, off_t i);
extern int      GetResourceUsage(double *usertime, double *systime);
extern void     Radix32(unsigned long, char *buff);
extern char *   ReadInDescriptor(int fd, struct stat *Sbp);
extern char *   ReadInFile(const char *name, struct stat *Sbp);
extern void     TempName(char *dir, char *buff);
extern FILE *   xfopena(const char *p);
extern bool     fdreserve(int fdnum);
extern FILE *   Fopen(const char *p, const char *type, int fd);
extern int      Fclose(FILE *fp);

extern int      argify(char *line, char ***argvp);
extern void     freeargify(char ***argvp);

extern const char *     Aliasgetnamebyhash(const HASH hash);
extern HASH             Aliasgethashbyhash(const HASH hash);
extern HASH             Aliasgethashbygroup(const char *group);
extern const char *     Aliasgetnamebygroup(const char *group);
extern bool             LoadGroupAliases(void);

END_DECLS

#endif /* LIBINN_H */
