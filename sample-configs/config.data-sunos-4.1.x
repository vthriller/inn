## Sample config.data for SunOS 4.1.x

##
##  1.  MAKE CONFIG PARAMETERS
##  Where the DBZ sources are, from C News.  INN has a (maybe old) copy.
#### =()<DBZDIR			@<DBZDIR>@>()=
DBZDIR			../dbz
##  If you have a parallel make, set this to "&"
#### =()<P			@<P>@>()=
P			
##  C pre-processor flags
#### =()<DEFS			@<DEFS>@>()=
DEFS			-I../include
##  C compiler
#### =()<CC			@<CC>@>()=
CC			gcc
##  Does your compiler properly do "char const *"? Pick DO DONT or DUNNO
#### =()<USE_CHAR_CONST		@<USE_CHAR_CONST>@>()=
USE_CHAR_CONST		DUNNO
##  C compiler flags
#### =()<CFLAGS			@<CFLAGS>@>()=
CFLAGS			$(DEFS) -g
##  C compiler flags to use when compiling dbz
#### =()<DBZCFLAGS			@<DBZCFLAGS>@>()=
DBZCFLAGS			$(CFLAGS)
##  What flags to use if profiling; -p or -pg, e.g.
#### =()<PROF			@<PROF>@>()=
PROF			-pg
##  Flags for the "cc -o" line; e.g., -Bstatic on SunOS4.x while debugging.
#### =()<LDFLAGS			@<LDFLAGS>@>()=
LDFLAGS			
##  If you use the standard NNTP way of connecting, where is the library?
#### =()<NNTPLIB		@<NNTPLIB>@>()=
NNTPLIB		
##  If you need to link in other libraries, add them here.
##  On NetBSD and FreeBSD, you must add the -lcrypt directive here
##  -lutil on BSD/OS gives you setproctitle() see HAVE_SETPROCTITLE.
#### =()<LIBS			@<LIBS>@>()=
LIBS			-lresolv
##  How to make a lint library; pick BSD, SYSV, or NONE.
#### =()<LINTLIBSTYLE		@<LINTLIBSTYLE>@>()=
LINTLIBSTYLE		NONE
##  Flags for lint.  AIX wants "-wkD"; it and others don't want "-z".
#### =()<LINTFLAGS		@<LINTFLAGS>@>()=
LINTFLAGS		-b -h $(DEFS)
##  Some lints insist on putting out the filename and other crap.
##  Possible values:
##	LINTFILTER              | sed -n -f ../sedf.aix
##	LINTFILTER              | sed -n -f ../sedf.osx
##	LINTFILTER              | sed -n -f ../sedf.sun
##	LINTFILTER              | sed -n -f ../sedf.sysv
##	LINTFILTER              
####  =()<LINTFILTER		@<LINTFILTER>@>()=
LINTFILTER		| sed -n -f ../sedf.sun
##  How to install manpages; pick SOURCE, NROFF-PACK, NROFF-PACK-SCO,
##  BSD4.4 or NONE.
#### =()<MANPAGESTYLE		@<MANPAGESTYLE>@>()=
MANPAGESTYLE		SOURCE
##  Where various manpages should go
#### =()<MAN1			@<MAN1>@>()=
MAN1			/usr/news/man/man1
#### =()<MAN3			@<MAN3>@>()=
MAN3			/usr/news/man/man3
#### =()<MAN5			@<MAN5>@>()=
MAN5			/usr/news/man/man5
#### =()<MAN8			@<MAN8>@>()=
MAN8			/usr/news/man/man8
##  Ranlib command.  Use echo if you don't need ranlib.
#### =()<RANLIB			@<RANLIB>@>()=
RANLIB			ranlib
##  YACC (yet another config control?)
#### =()<YACC			@<YACC>@>()=
YACC			yacc
##  Ctags command.  Use echo if you don't have ctags.
#### =()<CTAGS			@<CTAGS>@>()=
CTAGS			ctags -t -w


##
##  2.  LOGGING LEVELS
##  Facility innd should log under.
#### =()<LOG_INN_SERVER		@<LOG_INN_SERVER>@>()=
LOG_INN_SERVER		LOG_NEWS
##  Facility all other programs should log under.
#### =()<LOG_INN_PROG		@<LOG_INN_PROG>@>()=
LOG_INN_PROG		LOG_NEWS
##  Flags to use in opening the logs; some programs add LOG_PID.
#### =()<L_OPENLOG_FLAGS		@<L_OPENLOG_FLAGS>@>()=
L_OPENLOG_FLAGS		(LOG_CONS | LOG_NDELAY)
##  Log a fatal error; program is about to exit.
#### =()<L_FATAL			@<L_FATAL>@>()=
L_FATAL			LOG_CRIT
##  Log an error that might mean one or more articles get lost.
#### =()<L_ERROR			@<L_ERROR>@>()=
L_ERROR			LOG_ERR
##  Informational notice, usually not worth caring about.
#### =()<L_NOTICE		@<L_NOTICE>@>()=
L_NOTICE		LOG_WARNING
##  A protocol trace.
#### =()<L_TRACE			@<L_TRACE>@>()=
L_TRACE			LOG_DEBUG
##  All incoming control commands (ctlinnd, etc).
#### =()<L_CC_CMD		@<L_CC_CMD>@>()=
L_CC_CMD		LOG_INFO


##
##  3.  OWNERSHIPS AND FILE MODES
##  Owner of articles and directories and _PATH_INNDDIR
#### =()<NEWSUSER			@<NEWSUSER>@>()=
NEWSUSER			news
##  Group, for same purpose
#### =()<NEWSGROUP		@<NEWSGROUP>@>()=
NEWSGROUP		news
##  Who gets email from news.daily and about control messages.
#### =()<NEWSMASTER		@<NEWSMASTER>@>()=
NEWSMASTER		usenet
##  Who gets email on the Path line?
#### =()<PATHMASTER		@<PATHMASTER>@>()=
PATHMASTER		not-for-mail
##  Umask to set.
#### =()<NEWSUMASK		@<NEWSUMASK>@>()=
NEWSUMASK		02
##  Mode that incoming articles are created under.
#### =()<ARTFILE_MODE		@<ARTFILE_MODE>@>()=
ARTFILE_MODE		0664
##  Mode that batch files are created under.
#### =()<BATCHFILE_MODE		@<BATCHFILE_MODE>@>()=
BATCHFILE_MODE		0664
##  Mode that directories are created under.
#### =()<GROUPDIR_MODE		@<GROUPDIR_MODE>@>()=
GROUPDIR_MODE		0775


##
##  4.  C LIBRARY DIFFERENCES
##  Use stdargs, varargs, or neither?  Pick VARARGS STDARGS or NONE.
##  You need vfprintf and vfsprintf if not NONE.
#### =()<VAR_STYLE		@<VAR_STYLE>@>()=
VAR_STYLE		STDARGS
##  If you don't have <string.h>, set this to "mystring.h"
#### =()<STR_HEADER		@<STR_HEADER>@>()=
STR_HEADER		<string.h>
##  If you don't have <memory.h>, set this to "mymemory.h"
#### =()<MEM_HEADER		@<MEM_HEADER>@>()=
MEM_HEADER		<memory.h>
##  What is a file offset?  MUST BE LONG FOR NOW.
#### =()<OFFSET_T		@<OFFSET_T>@>()=
OFFSET_T		long
##  What is the type of an object size? Usually size_t or unsigned int.
#### =()<SIZE_T			@<SIZE_T>@>()=
SIZE_T			size_t
##  What is the type of a passwd uid and gid, for use in chown(2)?
#### =()<UID_T			@<UID_T>@>()=
UID_T			uid_t
#### =()<GID_T			@<GID_T>@>()=
GID_T			gid_t
##  Type of a pid, for use in kill(2).
#### =()<PID_T			@<PID_T>@>()=
PID_T			pid_t
##  Generic pointer, used by memcpy, malloc, etc.  Usually char or void.
#### =()<POINTER			@<POINTER>@>()=
POINTER			char
##  Worst-case alignment, in order to shut lint up
#### =()<ALIGNPTR		@<ALIGNPTR>@>()=
ALIGNPTR		int
##  What should a signal handler return?  Usually int or void.
#### =()<SIGHANDLER		@<SIGHANDLER>@>()=
SIGHANDLER		void
##  Type of variables can be modified in a signal handler? sig_atomic_t
#### =()<SIGVAR			@<SIGVAR>@>()=
SIGVAR			int
##  Function that returns no value, and a pointer to it.  Pick int or void
#### =()<FUNCTYPE		@<FUNCTYPE>@>()=
FUNCTYPE		void
##  Type of 32 bit unsigned integer.
#### =()<U_INT32_T			@<U_INT32_T>@>()=
U_INT32_T			unsigned int
##  Type of 32 bit signed integer.
#### =()<INT32_T			@<INT32_T>@>()=
INT32_T			int
##  Use BSD4.2 or Posix directory names?  Pick DIRENT or DIRECT.
#### =()<DIR_STYLE		@<DIR_STYLE>@>()=
DIR_STYLE		DIRENT
##  Use flock, lockf, or nothing to lock files?
##  Pick FLOCK, LOCKF, FCNTL, or NONE
#### =()<LOCK_STYLE		@<LOCK_STYLE>@>()=
LOCK_STYLE		FLOCK
##  Do you have <unistd.h>?  Pick DO or DONT
#### =()<HAVE_UNISTD		@<HAVE_UNISTD>@>()=
HAVE_UNISTD		DO
##  Do you have setbuffer?  Pick DO or DONT.
#### =()<HAVE_SETBUFFER		@<HAVE_SETBUFFER>@>()=
HAVE_SETBUFFER		DO
##  Do you have gettimeofday?  Pick DO or DONT.
#### =()<HAVE_GETTIMEOFDAY	@<HAVE_GETTIMEOFDAY>@>()=
HAVE_GETTIMEOFDAY	DO
##  Do you have setproctitle()?  Pick DO or DONT.
##  You may need to adjust LIBS for this.
#### =()<HAVE_SETPROCTITLE	@<HAVE_SETPROCTITLE>@>()=
HAVE_SETPROCTITLE	DONT
##  Do you have fchmod?  Pick DO or DONT.
#### =()<HAVE_FCHMOD		@<HAVE_FCHMOD>@>()=
HAVE_FCHMOD		DO
##  Do you have setsid()?  Pick DO or DONT.
#### =()<HAVE_SETSID		@<HAVE_SETSID>@>()=
HAVE_SETSID		DO
##  Does your (struct tm) have a tm_gmtoff field?  Pick DO or DONT.
#### =()<HAVE_TM_GMTOFF		@<HAVE_TM_GMTOFF>@>()=
HAVE_TM_GMTOFF		DO
##  Does your (struct stat) have a st_blksize field?  Pick DO or DONT.
#### =()<HAVE_ST_BLKSIZE		@<HAVE_ST_BLKSIZE>@>()=
HAVE_ST_BLKSIZE		DO
##  Use waitpid instead of wait3?  Pick DO or DONT.
#### =()<HAVE_WAITPID		@<HAVE_WAITPID>@>()=
HAVE_WAITPID		DO
##  Use "union wait" instead of int?  Pick DO or DONT.
#### =()<USE_UNION_WAIT		@<USE_UNION_WAIT>@>()=
USE_UNION_WAIT		DONT
##  How to fork?  Pick fork or vfork.
#### =()<FORK			@<FORK>@>()=
FORK			fork
##  Do you have <vfork.h>?  Pick DO or DONT.
#### =()<HAVE_VFORK		@<HAVE_VFORK>@>()=
HAVE_VFORK		DONT
##  Do you have symbolic links?  Pick DO or DONT.
#### =()<HAVE_SYMLINK		@<HAVE_SYMLINK>@>()=
HAVE_SYMLINK		DO
##  Do you have Unix-domain sockets?  Pick DO or DONT.
#### =()<HAVE_UNIX_DOMAIN	@<HAVE_UNIX_DOMAIN>@>()=
HAVE_UNIX_DOMAIN	DO
##  Does your AF_UNIX bind use sizeof for the socket size?  Pick DO or DONT.
#### =()<BIND_USE_SIZEOF		@<BIND_USE_SIZEOF>@>()=
BIND_USE_SIZEOF		DO
##  How should close-on-exec be done?  Pick IOCTL or FCNTL.
#### =()<CLX_STYLE		@<CLX_STYLE>@>()=
CLX_STYLE		IOCTL
##  How should non-blocking I/O be done?  Pick IOCTL or FCNTL.
#### =()<NBIO_STYLE		@<NBIO_STYLE>@>()=
NBIO_STYLE		FCNTL
##  How should resource-totalling be done?  Pick RUSAGE or TIMES
#### =()<RES_STYLE		@<RES_STYLE>@>()=
RES_STYLE		RUSAGE
##  How to get number of available descriptors?
##  Pick GETDTAB, GETRLIMIT, SYSCONF, ULIMIT, or CONSTANT.
####  =()<FDCOUNT_STYLE		@<FDCOUNT_STYLE>@>()=
FDCOUNT_STYLE		GETRLIMIT
##  If greater than -1, then use [gs]etrlimit to set that many descriptors.
##  If -1, then no [gs]etrlimit calls are done.
#### =()<NOFILE_LIMIT		@<NOFILE_LIMIT>@>()=
NOFILE_LIMIT		126
##  Do you need <time.h> as well as <sys/time.h>?  Pick DO or DONT.
#### =()<NEED_TIME		@<NEED_TIME>@>()=
NEED_TIME		DONT
##  What predicate, if any, the <ctype.h> macros need
#### =()<CTYPE			@<CTYPE>@>()=
CTYPE			(isascii((c)) && isXXXXX((c)))
#CTYPE			((c) > 0 && isXXXXX((c)))
#CTYPE			isXXXXX((c))
##  What's the return type of abort?  Usually int or void.
#### =()<ABORTVAL		@<ABORTVAL>@>()=
ABORTVAL		int
##  What's the return type of alarm?  Usually int or unsigned int.
#### =()<ALARMVAL		@<ALARMVAL>@>()=
ALARMVAL		unsigned int
##  What's the return type of getpid?  Usually int or unsigned int.
#### =()<GETPIDVAL		@<GETPIDVAL>@>()=
GETPIDVAL		pid_t
##  What's the return type of sleep?  Usually int or unsigned int.
#### =()<SLEEPVAL		@<SLEEPVAL>@>()=
SLEEPVAL		unsigned int
##  What's the return type of  qsort?  Usually int or void.
#### =()<QSORTVAL		@<QSORTVAL>@>()=
QSORTVAL		int
##  What's the return type of lseek?  Usually long or off_t.
#### =()<LSEEKVAL		@<LSEEKVAL>@>()=
LSEEKVAL		off_t
##  What's the return type of free?  Usually int or void.
#### =()<FREEVAL			@<FREEVAL>@>()=
FREEVAL			int
##  What's the return type of exit?  Usually int or void.
##  (For gcc (not pedantic ANSI) use "volatile void" in EXITVAL and _EXITVAL.)
#### =()<EXITVAL			@<EXITVAL>@>()=
EXITVAL			void
##  What's the return type of _exit?  Usually int or void.
#### =()<_EXITVAL		@<_EXITVAL>@>()=
_EXITVAL		void


##
##  5.  C LIBRARY OMISSIONS
##  Possible values:
##	MISSING_MAN		strcasecmp.3 syslog.3
##	MISSING_SRC		strcasecmp.c syslog.c strerror.c getdtab.c
##	MISSING_OBJ		strcasecmp.o syslog.o strerror.o getdtab.c
##  getdtab has a getdtablesize() routine if you need it; see the lib
##  directory and Install.ms for others.
##  OSx systems should add $(OSXATTOBJ) to MISSING_OBJ.
#### =()<MISSING_MAN		@<MISSING_MAN>@>()=
MISSING_MAN		
#### =()<MISSING_SRC		@<MISSING_SRC>@>()=
MISSING_SRC		strerror.c mktemp.c memmove.c
#### =()<MISSING_OBJ		@<MISSING_OBJ>@>()=
MISSING_OBJ		strerror.o mktemp.o memmove.o


##
##  6.  MISCELLANEOUS CONFIG DATA
##
##  Use read/write to update the active file, or mmap?  Pick READ or MMAP.
#### =()<ACT_STYLE		@<ACT_STYLE>@>()=
ACT_STYLE		MMAP
##  Do you want mail notifications of bad control messages. DO or DONT
##  Setting it to do results in a lot of mail with the number of spammers
##  out there.
#### =()<MAIL_BADCONTROLS	@<MAIL_BADCONTROLS>@>()=
MAIL_BADCONTROLS	DONT
##  What type of pointer does mmap() manage? Normally ``caddr_t'' or ``void *''
##  or ``char *''
#### =()<MMAP_PTR		@<MMAP_PTR>@>()=
MMAP_PTR		caddr_t
##  Should we msync when using mmap? Pick DO or DONT. Useful
##  with some slightly broken mmap implementations. (like HPUX and BSD/OS).
#### =()<MMAP_SYNC		@<MMAP_SYNC>@>()=
MMAP_SYNC		DONT
##  Should we call your msync() with 3 args? (addr,len,flags)
##  Choose DO or DONT.
#### =()<MSYNC_3_ARG		@<MSYNC_3_ARG>@>()=
MSYNC_3_ARG		DONT
##  Do clients use our NNTP-server-open routine, or the one in NNTP?
##  INND is nicer, but you must install inn.conf files everywhere; NNTP
##  is better if you already have lots of /usr/lib/news/server files.
##  Pick INND or NNTP.
#### =()<REM_STYLE		@<REM_STYLE>@>()=
REM_STYLE		INND
##  Should rnews save articles that the server rejects?  Pick DO or DONT.
#### =()<RNEWS_SAVE_BAD		@<RNEWS_SAVE_BAD>@>()=
RNEWS_SAVE_BAD		DONT
##  Should rnews log articles innd already has?  Pick SYSLOG, FILE, OR DONT.
#### =()<RNEWS_LOG_DUPS		@<RNEWS_LOG_DUPS>@>()=
RNEWS_LOG_DUPS		DONT
##  Look in _PATH_RNEWSPROGS for rnews unpackers?  Pick DO or DONT.
#### =()<RNEWSPROGS		@<RNEWSPROGS>@>()=
RNEWSPROGS		DO
##  Should rnews try the local host?  Pick DO or DONT.
#### =()<RNEWSLOCALCONNECT	@<RNEWSLOCALCONNECT>@>()=
RNEWSLOCALCONNECT	DO
##  Environment variable that has remote hostname for rnews.
#### =()<_ENV_UUCPHOST		@<_ENV_UUCPHOST>@>()=
_ENV_UUCPHOST		UU_MACHINE
##  Require posts to have under 50% inclusion (">") lines?  Pick DO OR DONT.
##  (This is only for inews and nnrpd.)
#### =()<CHECK_INCLUDED_TEXT	@<CHECK_INCLUDED_TEXT>@>()=
CHECK_INCLUDED_TEXT	DO
##  Put hosts in the inews Path header?  Pick DO or DONT.
#### =()<INEWS_PATH		@<INEWS_PATH>@>()=
INEWS_PATH		DO
##  Munge the gecos field of password entry?  Pick DO or DONT.
#### =()<MUNGE_GECOS		@<MUNGE_GECOS>@>()=
MUNGE_GECOS		DO
##  How many times to try to fork before giving up
#### =()<MAX_FORKS		@<MAX_FORKS>@>()=
MAX_FORKS		10
##  Largest acceptable article size; 0 allows any size
#### =()<MAX_ART_SIZE		@<MAX_ART_SIZE>@>()=
MAX_ART_SIZE		1000000L
##  Value of dbzincore(FLAG) call in innd.  Pick 1 or 0.
#### =()<INND_DBZINCORE		@<INND_DBZINCORE>@>()=
INND_DBZINCORE		1
##  Should sub-processes get a nice(2) value?  Pick DO or DONT.
#### =()<INND_NICE_KIDS		@<INND_NICE_KIDS>@>()=
INND_NICE_KIDS		DO
##  Value for nice(2) call in innd.
#### =()<INND_NICE_VALUE		@<INND_NICE_VALUE>@>()=
INND_NICE_VALUE		4
##  Null-terminated list of unknown commands to not log to syslog.
##	INND_QUIET_BADLIST	"xstream", "xfoo", NULL
#### =()<INND_QUIET_BADLIST	@<INND_QUIET_BADLIST>@>()=
INND_QUIET_BADLIST	NULL
##  Null-terminated set of illegal distribution patterns for local postings.
#### =()<BAD_DISTRIBS		@<BAD_DISTRIBS>@>()=
BAD_DISTRIBS		"*.*",NULL
##  Verify that the poster is the person doing the cancel?  Pick DO or DONT.
##  (Can't do this if cancel arrives before the article does, by the way,
##  in which case the early cancel will be ignored.)
#### =()<VERIFY_CANCELS		@<VERIFY_CANCELS>@>()=
VERIFY_CANCELS		DONT
##  Log "ctlinnd cancel" commands to syslog?  Pick DO or DONT.
#### =()<LOG_CANCEL_COMMANDS	@<LOG_CANCEL_COMMANDS>@>()=
LOG_CANCEL_COMMANDS	DONT
##  File unknown "to.*" groups into the "to" newsgroup?  Pick DO or DONT.
#### =()<MERGE_TO_GROUPS		@<MERGE_TO_GROUPS>@>()=
MERGE_TO_GROUPS		DONT
##  File articles in unknown newsgroups into junk?  Pick DO or DONT.
#### =()<WANT_TRASH		@<WANT_TRASH>@>()=
WANT_TRASH		DONT
##  Record rejected articles in history?  Pick DO or DONT.
#### =()<REMEMBER_TRASH		@<REMEMBER_TRASH>@>()=
REMEMBER_TRASH		DO
##  Check the linecount against the Lines header?  Pick DO or DONT.
#### =()<CHECK_LINECOUNT		@<CHECK_LINECOUNT>@>()=
CHECK_LINECOUNT		DONT
##  If checking, the error must be within LINECOUNT_FUZZ lines.
##  Five is number of .signature lines + 1.
#### =()<LINECOUNT_FUZZ		@<LINECOUNT_FUZZ>@>()=
LINECOUNT_FUZZ		5
##  Have innd throttle itself after this many I/O errors.
#### =()<IO_ERROR_COUNT		@<IO_ERROR_COUNT>@>()=
IO_ERROR_COUNT		50
##  Default value for ctlinnd -t flag; use 0 to wait and poll.
#### =()<CTLINND_TIMEOUT		@<CTLINND_TIMEOUT>@>()=
CTLINND_TIMEOUT		0
##  Flush logs if we go this long with no I/O.
#### =()<DEFAULT_TIMEOUT		@<DEFAULT_TIMEOUT>@>()=
DEFAULT_TIMEOUT		300
##  INND closes channel if inactive this long (seconds).
#### =()<PEER_TIMEOUT		@<PEER_TIMEOUT>@>()=
PEER_TIMEOUT		(1 * 60 * 60)
##  NNRP exits if first command doesn't arrive within this time (seconds).
#### =()<INIT_CLIENT_TIMEOUT	@<INIT_CLIENT_TIMEOUT>@>()=
INIT_CLIENT_TIMEOUT	30
##  NNRP exits if inactive for this long (seconds).
#### =()<CLIENT_TIMEOUT		@<CLIENT_TIMEOUT>@>()=
CLIENT_TIMEOUT		(10 * 60)
##  Allow nnrpd readers when paused or throttled?  Pick DO or DONT.
#### =()<ALLOW_READERS		@<ALLOW_READERS>@>()=
ALLOW_READERS		DO
##  Refuse newsreader connections if load is higher then this; -1 disables.
#### =()<NNRP_LOADLIMIT		@<NNRP_LOADLIMIT>@>()=
NNRP_LOADLIMIT		16
##  Don't readdir() spool dir if same group within this many secs.
#### =()<NNRP_RESCAN_DELAY	@<NNRP_RESCAN_DELAY>@>()=
NNRP_RESCAN_DELAY	60
##  Do gethostbyaddr on client adresses in nnrp?   Pick DO or DONT.
##  (If DONT, then use only IP addresses in hosts.nnrp)
#### =()<NNRP_GETHOSTBYADDR	@<NNRP_GETHOSTBYADDR>@>()=
NNRP_GETHOSTBYADDR	DO
##  How many Message-ID retrievals until nnrpd does a dbzincore?  Set
##  to -1 to never do incore.
#### =()<NNRP_DBZINCORE_DELAY	@<NNRP_DBZINCORE_DELAY>@>()=
NNRP_DBZINCORE_DELAY	40
##  Strip Sender from posts that didn't authenticate?  Pick DO or DONT.
#### =()<NNRP_AUTH_SENDER	@<NNRP_AUTH_SENDER>@>()=
NNRP_AUTH_SENDER	DO
##  Do you want to make life easy for peers to pull feeds from you (it's
##  harder on your machine). Pick DO or DONT. With DONT, you get a small
##  sleep inserted before each ARTICLE command is processed. (With
##  apologies to the Australians in the audience).
#### =()<LIKE_PULLERS		@<LIKE_PULLERS>@>()=
LIKE_PULLERS		DONT
##  Allow the NEWNEWS NNTP command?   Pick DO or DONT.
##  (RFC 977 says you should; your server performance may not agree...)
#### =()<ALLOW_NEWNEWS		@<ALLOW_NEWNEWS>@>()=
ALLOW_NEWNEWS		DONT
##  How many read/write failures until channel is put to sleep or closed?
#### =()<BAD_IO_COUNT		@<BAD_IO_COUNT>@>()=
BAD_IO_COUNT		5
##  Multiplier for sleep in EWOULDBLOCK writes (seconds).
#### =()<BLOCK_BACKOFF		@<BLOCK_BACKOFF>@>()=
BLOCK_BACKOFF		(2 * 60)
##  How many article-writes between active and history updates?
#### =()<ICD_SYNC_COUNT		@<ICD_SYNC_COUNT>@>()=
ICD_SYNC_COUNT		10
##  Tell resolver _res.options to be fast?  Pick DO or DONT.
#### =()<FAST_RESOLV		@<FAST_RESOLV>@>()=
FAST_RESOLV		DONT
##  Drop articles that were posted this many days ago.
#### =()<DEFAULT_CUTOFF		@<DEFAULT_CUTOFF>@>()=
DEFAULT_CUTOFF		14
##  Maximum number of incoming NNTP connections.
#### =()<DEFAULT_CONNECTIONS	@<DEFAULT_CONNECTIONS>@>()=
DEFAULT_CONNECTIONS	50
##  Wait this many seconds before channel restarts.
#### =()<CHANNEL_RETRY_TIME	@<CHANNEL_RETRY_TIME>@>()=
CHANNEL_RETRY_TIME	(5 * 60)
##  Wait this many seconds before seeing if pause is ended.
#### =()<PAUSE_RETRY_TIME	@<PAUSE_RETRY_TIME>@>()=
PAUSE_RETRY_TIME	(5 * 60)
##  Wait this many seconds between noticing inactive channels.
#### =()<CHANNEL_INACTIVE_TIME	@<CHANNEL_INACTIVE_TIME>@>()=
CHANNEL_INACTIVE_TIME	(10 * 60)
##  Put nntplink info (filename) into the log?
#### =()<NNTPLINK_LOG		@<NNTPLINK_LOG>@>()=
NNTPLINK_LOG		DONT
##  Put article size into the log?
#### =()<LOG_SIZE		@<LOG_SIZE>@>()=
LOG_SIZE		DO
##  Log by host IP address, rather than from Path line?
#### =()<IPADDR_LOG		@<IPADDR_LOG>@>()=
IPADDR_LOG		DO
##  Log NNTP activity after this many articles.
#### =()<NNTP_ACTIVITY_SYNC	@<NNTP_ACTIVITY_SYNC>@>()=
NNTP_ACTIVITY_SYNC	200
##  Free buffers bigger than this when we're done with them.
#### =()<BIG_BUFFER		@<BIG_BUFFER>@>()=
BIG_BUFFER		(2 * START_BUFF_SIZE)
##  A general small buffer.
#### =()<SMBUF			@<SMBUF>@>()=
SMBUF			256
##  Buffer for a single article name.
#### =()<MAXARTFNAME		@<MAXARTFNAME>@>()=
MAXARTFNAME		10
##  Buffer for a single pathname in the spool directory.
#### =()<SPOOLNAMEBUFF		@<SPOOLNAMEBUFF>@>()=
SPOOLNAMEBUFF		512
##  Maximum size of a single header.
#### =()<MAXHEADERSIZE		@<MAXHEADERSIZE>@>()=
MAXHEADERSIZE		1024
##  Byte limit on locally-posted articles; 0 to disable the check.
#### =()<LOCAL_MAX_ARTSIZE	@<LOCAL_MAX_ARTSIZE>@>()=
LOCAL_MAX_ARTSIZE	1000000L
##  Default number of bytes to hold in memory when buffered.
#### =()<SITE_BUFFER_SIZE	@<SITE_BUFFER_SIZE>@>()=
SITE_BUFFER_SIZE	(16 * 1024)
##  Do you have uustat, or just uuq?  Pick DO or DONT
#### =()<HAVE_UUSTAT		@<HAVE_UUSTAT>@>()=
HAVE_UUSTAT		DO
##  Should INN do some setsockopts on network connections. Pick DO or DONT.
##  Some versions of Solaris should set to DONT (pre 2.4 it seems)
#### =()<SET_SOCKOPT		@<SET_SOCKOPT>@>()=
SET_SOCKOPT		DO


##
##  7.  PATHS TO COMMON PROGRAMS
##  Where the raison d'etre for this distribution lives.
#### =()<_PATH_INND		@<_PATH_INND>@>()=
_PATH_INND		/usr/news/bin/innd
##  Where the optional front-end that exec's innd lives.
#### =()<_PATH_INNDSTART		@<_PATH_INNDSTART>@>()=
_PATH_INNDSTART		/usr/news/bin/inndstart
##  Where news boot-up script should be installed.
#### =()<_PATH_NEWSBOOT		@<_PATH_NEWSBOOT>@>()=
_PATH_NEWSBOOT		/usr/news/bin/rc.news
##  Where sendmail, or a look-alike, lives.
##  The -t is optional and says to read message for recipients
#### =()<_PATH_SENDMAIL		@<_PATH_SENDMAIL>@>()=
_PATH_SENDMAIL		/usr/lib/sendmail -t
##  Where the shell is.
#### =()<_PATH_SH		@<_PATH_SH>@>()=
_PATH_SH		/bin/sh
##  Where the compress program lives.
#### =()<_PATH_COMPRESS		@<_PATH_COMPRESS>@>()=
_PATH_COMPRESS		/usr/bin/compress
##  What extension your compress appends
#### =()<_PATH_COMPRESSEXT	@<_PATH_COMPRESSEXT>@>()=
_PATH_COMPRESSEXT	.Z
##  Where egrep lives (you might need the FSF one; see scanlogs)
#### =()<_PATH_EGREP		@<_PATH_EGREP>@>()=
_PATH_EGREP		/usr/bin/egrep
##  Where perl lives
#### =()<_PATH_PERL		@<_PATH_PERL>@>()=
_PATH_PERL		/usr/bin/perl
##  Where awk lives
#### =()<_PATH_AWK		@<_PATH_AWK>@>()=
_PATH_AWK		/usr/bin/nawk
##  Where sed lives (you might need the FSF one)
#### =()<_PATH_SED		@<_PATH_SED>@>()=
_PATH_SED		/usr/bin/sed
##  Where sort lives (you may want the FSF one).
#### =()<_PATH_SORT		@<_PATH_SORT>@>()=
_PATH_SORT		/usr/bin/sort
##  Where inews lives.
#### =()<_PATH_INEWS		@<_PATH_INEWS>@>()=
_PATH_INEWS		/usr/news/bin/inews
##  Where rnews lives.
#### =()<_PATH_RNEWS		@<_PATH_RNEWS>@>()=
_PATH_RNEWS		/usr/news/bin/rnews
##  Where generic authentication programs live.
#### =()<_PATH_AUTHDIR		@<_PATH_AUTHDIR>@>()=
_PATH_AUTHDIR		/usr/news/bin/auth
##  Where the NNRP server lives.
#### =()<_PATH_NNRPD		@<_PATH_NNRPD>@>()=
_PATH_NNRPD		/usr/news/bin/nnrpd
##  The path of the process run when an unknown host connects to innd.
##  Usually the same as _PATH_NNRPD, but may be, e.g., the path to
##  nntpd from the reference implementation.
#### =()<_PATH_NNTPD		@<_PATH_NNTPD>@>()=
_PATH_NNTPD		/usr/news/bin/nnrpd
##  Where most other programs live.
##  See also _PATH_RNEWSPROGS and _PATH_CONTROLPROGS, below.
#### =()<_PATH_NEWSBIN		@<_PATH_NEWSBIN>@>()=
_PATH_NEWSBIN		/usr/news/bin
##  Where temporary files live on the server
#### =()<_PATH_TMP		@<_PATH_TMP>@>()=
_PATH_TMP		/var/tmp
##  Command to send mail (with -s "subject" allowed)
#### =()<_PATH_MAILCMD		@<_PATH_MAILCMD>@>()=
_PATH_MAILCMD		/usr/bin/Mail
##  Where scripts should have shlock create locks.
#### =()<_PATH_LOCKS		@<_PATH_LOCKS>@>()=
_PATH_LOCKS		/var/news/locks
##  Where your GNU gzip binary is (for rnews to run on gzipped batches).
#### =()<_PATH_GZIP		@<_PATH_GZIP>@>()=
_PATH_GZIP		/usr/contrib/bin/gzip

##
##  8.  PATHS RELATED TO THE SPOOL DIRECTORY
##  Spool directory, where articles live.
#### =()<_PATH_SPOOL		@<_PATH_SPOOL>@>()=
_PATH_SPOOL		/var/news/spool/articles
##  Spool directory where overview data lives.
#### =()<_PATH_OVERVIEWDIR	@<_PATH_OVERVIEWDIR>@>()=
_PATH_OVERVIEWDIR	/var/news/spool/over.view
##  Name of overview file within its spool directory.
#### =()<_PATH_OVERVIEW		@<_PATH_OVERVIEW>@>()=
_PATH_OVERVIEW		.overview
##  Where rnews spools its input.
#### =()<_PATH_SPOOLNEWS		@<_PATH_SPOOLNEWS>@>()=
_PATH_SPOOLNEWS		/var/news/spool/in.coming
##  Where rnews creates temporary files until finished
#### =()<_PATH_SPOOLTEMP		@<_PATH_SPOOLTEMP>@>()=
_PATH_SPOOLTEMP		/var/tmp
##  Where rnews puts bad input.
#### =()<_PATH_BADNEWS		@<_PATH_BADNEWS>@>()=
_PATH_BADNEWS		/var/news/spool/in.coming/bad
##  Where rnews puts bad input, relative to _PATH_SPOOLNEWS.
#### =()<_PATH_RELBAD		@<_PATH_RELBAD>@>()=
_PATH_RELBAD		bad
## Where the XBATCH command should put the batches. Normally same as
## _PATH_SPOOLNEWS so that 'rnews -U' can find it.
#### =()<_PATH_XBATCHES		@<_PATH_XBATCHES>@>()=
_PATH_XBATCHES		/var/news/spool/in.coming


##
##  9.  EXECUTION PATHS FOR INND AND RNEWS
##  Pathname where dups are logged if RNEWS_LOG_DUPS is FILE.
#### =()<_PATH_RNEWS_DUP_LOG	@<_PATH_RNEWS_DUP_LOG>@>()=
_PATH_RNEWS_DUP_LOG	/dev/null
##  Rnews may execute any program in this directory; see RNEWSPROGS.
#### =()<_PATH_RNEWSPROGS	@<_PATH_RNEWSPROGS>@>()=
_PATH_RNEWSPROGS	/usr/news/bin/rnews.libexec
##  Path to control messages scripts.
#### =()<_PATH_CONTROLPROGS	@<_PATH_CONTROLPROGS>@>()=
_PATH_CONTROLPROGS	/usr/news/bin/control
##  Default "unknown/illegal" control script, within _PATH_CONTROLPROGS.
#### =()<_PATH_BADCONTROLPROG	@<_PATH_BADCONTROLPROG>@>()=
_PATH_BADCONTROLPROG	default


##
##  10.  SOCKETS CREATED BY INND OR CLIENTS
#### =()<_PATH_INNDDIR		@<_PATH_INNDDIR>@>()=
_PATH_INNDDIR		/var/news/run
##  Unix-domain stream socket that rnews connects to.
#### =()<_PATH_NNTPCONNECT	@<_PATH_NNTPCONNECT>@>()=
_PATH_NNTPCONNECT	/var/news/run/nntpin
##  Unix-domain datagram socket that ctlinnd to.
#### =()<_PATH_NEWSCONTROL	@<_PATH_NEWSCONTROL>@>()=
_PATH_NEWSCONTROL	/var/news/run/control
##  Temporary socket created by ctlinnd; run through mktemp
#### =()<_PATH_TEMPSOCK		@<_PATH_TEMPSOCK>@>()=
_PATH_TEMPSOCK		/var/news/run/ctlinndXXXXXX


##
##  11.  LOG AND CONFIG FILES
##  Shell script that sets most of these as shell vars
#### =()<_PATH_SHELLVARS		@<_PATH_SHELLVARS>@>()=
_PATH_SHELLVARS		/var/news/etc/innshellvars
##  Perl script that sets most of these as perl variables
#### =()<_PATH_PERL_SHELLVARS	@<_PATH_PERL_SHELLVARS>@>()=
_PATH_PERL_SHELLVARS	/var/news/etc/innshellvars.pl
##  TCL script that sets most of these as tcl variables
#### =()<_PATH_TCL_SHELLVARS	@<_PATH_TCL_SHELLVARS>@>()=
_PATH_TCL_SHELLVARS	/var/news/etc/innshellvars.tcl
##  csh script that sets most of these as csh variables
#### =()<_PATH_CSH_SHELLVARS	@<_PATH_CSH_SHELLVARS>@>()=
_PATH_CSH_SHELLVARS	/var/news/etc/innshellvars.csh
##  Where most config and data files are usually stored; not required
##  to the home directory of NEWSUSER.
#### =()<_PATH_NEWSLIB		@<_PATH_NEWSLIB>@>()=
_PATH_NEWSLIB		/var/news/etc
## The group owner of the rnews program. Usually uucp.
#### =()<RNEWS_GROUP		@<RNEWS_GROUP>@>()=
RNEWS_GROUP		uucp
##  The server's log file.
#### =()<_PATH_LOGFILE		@<_PATH_LOGFILE>@>()=
_PATH_LOGFILE		/var/log/news/news
##  The server's error log file.
#### =()<_PATH_ERRLOG		@<_PATH_ERRLOG>@>()=
_PATH_ERRLOG		/var/log/news/errlog
##  Where most sylog log files go; see also scanlogs, innstat, etc.
#### =()<_PATH_MOST_LOGS		@<_PATH_MOST_LOGS>@>()=
_PATH_MOST_LOGS		/var/log/news
##  How many generates of log files to keep.
#### =()<LOG_CYCLES		@<LOG_CYCLES>@>()=
LOG_CYCLES		3
##  Text value of the server's pid.
#### =()<_PATH_SERVERPID		@<_PATH_SERVERPID>@>()=
_PATH_SERVERPID		/var/news/run/innd.pid
##  The newsfeeds file, on the server host.
#### =()<_PATH_NEWSFEEDS		@<_PATH_NEWSFEEDS>@>()=
_PATH_NEWSFEEDS		/var/news/etc/newsfeeds
##  The article history database, on the server host.
#### =()<_PATH_HISTORY		@<_PATH_HISTORY>@>()=
_PATH_HISTORY		/var/news/etc/history
##  File listing the sites that feed us news.
#### =()<_PATH_INNDHOSTS		@<_PATH_INNDHOSTS>@>()=
_PATH_INNDHOSTS		/var/news/etc/hosts.nntp
##  The active file, on the server host.
#### =()<_PATH_ACTIVE		@<_PATH_ACTIVE>@>()=
_PATH_ACTIVE		/var/news/etc/active
##  A temporary active file, for writing on the server host.
#### =()<_PATH_NEWACTIVE		@<_PATH_NEWACTIVE>@>()=
_PATH_NEWACTIVE		/var/news/etc/active.tmp
##  An old active file on the server host.
#### =()<_PATH_OLDACTIVE		@<_PATH_OLDACTIVE>@>()=
_PATH_OLDACTIVE		/var/news/etc/active.old
##  The log of when groups are created.
#### =()<_PATH_ACTIVETIMES	@<_PATH_ACTIVETIMES>@>()=
_PATH_ACTIVETIMES	/var/news/etc/active.times
##  Where batch files are located.
#### =()<_PATH_BATCHDIR		@<_PATH_BATCHDIR>@>()=
_PATH_BATCHDIR		/var/news/spool/out.going
##  Where archives are kept.
#### =()<_PATH_ARCHIVEDIR	@<_PATH_ARCHIVEDIR>@>()=
_PATH_ARCHIVEDIR	/var/news/spool/archive
##  Where NNRP distributions file is
#### =()<_PATH_NNRPDIST		@<_PATH_NNRPDIST>@>()=
_PATH_NNRPDIST		/var/news/etc/distributions
##  Where the NNRP automatic subscriptions file is
#### =()<_PATH_NNRPSUBS		@<_PATH_NNRPSUBS>@>()=
_PATH_NNRPSUBS		/var/news/etc/subscriptions
##  Where the default Distribution assignments file is
#### =()<_PATH_DISTPATS		@<_PATH_DISTPATS>@>()=
_PATH_DISTPATS		/var/news/etc/distrib.pats
#### =()<_PATH_NEWSGROUPS	@<_PATH_NEWSGROUPS>@>()=
_PATH_NEWSGROUPS	/var/news/etc/newsgroups
##  File where client configuration parameters can be read.
#### =()<_PATH_CONFIG		@<_PATH_CONFIG>@>()=
_PATH_CONFIG		/var/news/etc/inn.conf
##  The possible active file, on clients (NFS-mounted, e.g.).
#### =()<_PATH_CLIENTACTIVE	@<_PATH_CLIENTACTIVE>@>()=
_PATH_CLIENTACTIVE	/var/news/etc/active
##  A temporary file, for client inews to use.
#### =()<_PATH_TEMPACTIVE	@<_PATH_TEMPACTIVE>@>()=
_PATH_TEMPACTIVE	/var/tmp/activeXXXXXX
##  Where to mail to the moderators.
#### =()<_PATH_MODERATORS	@<_PATH_MODERATORS>@>()=
_PATH_MODERATORS	/var/news/etc/moderators
##  A temporary file, for client inews to use.
#### =()<_PATH_TEMPMODERATORS	@<_PATH_TEMPMODERATORS>@>()=
_PATH_TEMPMODERATORS	/var/tmp/moderatorsXXXXXX
##  Where NNTP puts the name of the server.
#### =()<_PATH_SERVER		@<_PATH_SERVER>@>()=
_PATH_SERVER		/var/news/etc/server
##  File with name/password for all remote connections.
#### =()<_PATH_NNTPPASS		@<_PATH_NNTPPASS>@>()=
_PATH_NNTPPASS		/var/news/etc/passwd.nntp
##  NNRP access file.
#### =()<_PATH_NNRPACCESS	@<_PATH_NNRPACCESS>@>()=
_PATH_NNRPACCESS	/var/news/etc/nnrp.access
##  Default expire control file.
#### =()<_PATH_EXPIRECTL		@<_PATH_EXPIRECTL>@>()=
_PATH_EXPIRECTL		/var/news/etc/expire.ctl
##  Prolog to parse control scripts
#### =()<_PATH_PARSECTL		@<_PATH_PARSECTL>@>()=
_PATH_PARSECTL		/var/news/etc/parsecontrol
##  Access control file for control scripts.
#### =()<_PATH_CONTROLCTL	@<_PATH_CONTROLCTL>@>()=
_PATH_CONTROLCTL	/var/news/etc/control.ctl
##  Innwatch control file.
#### =()<_PATH_CTLWATCH		@<_PATH_CTLWATCH>@>()=
_PATH_CTLWATCH		/var/news/etc/innwatch.ctl
##  Where innwatch writes its own pid.
#### =()<_PATH_WATCHPID		@<_PATH_WATCHPID>@>()=
_PATH_WATCHPID		/var/news/run/innwatch.pid
##  Where innwatch writes status when it gets an interrupt
#### =()<_PATH_INNWSTATUS	@<_PATH_INNWSTATUS>@>()=
_PATH_INNWSTATUS	/var/news/run/innwatch.status
##  Format of news overview database
#### =()<_PATH_SCHEMA		@<_PATH_SCHEMA>@>()=
_PATH_SCHEMA		/var/news/etc/overview.fmt


##
##  12.  INNWATCH CONFIGURATION
##  Load average (* 100) at which innd should be paused.
#### =()<INNWATCH_PAUSELOAD	@<INNWATCH_PAUSELOAD>@>()=
INNWATCH_PAUSELOAD	1500
##  Load average (* 100) at which innd should be throttled.
#### =()<INNWATCH_HILOAD		@<INNWATCH_HILOAD>@>()=
INNWATCH_HILOAD		2000
##  Load average (* 100) at which to restart innd (pause/throttle undone).
#### =()<INNWATCH_LOLOAD		@<INNWATCH_LOLOAD>@>()=
INNWATCH_LOLOAD		1000
##  Space, in df output units, at which to throttle innd on _PATH_SPOOL 
##  or _PATH_OVERVIEWDIR.
#### =()<INNWATCH_SPOOLSPACE	@<INNWATCH_SPOOLSPACE>@>()=
INNWATCH_SPOOLSPACE	8000
##  Space, in df output units, at which to throttle innd on _PATH_BATCHDIR.
#### =()<INNWATCH_BATCHSPACE	@<INNWATCH_BATCHSPACE>@>()=
INNWATCH_BATCHSPACE	800
##  Space, in df output units, at which to throttle innd on _PATH_NEWSLIB.
#### =()<INNWATCH_LIBSPACE	@<INNWATCH_LIBSPACE>@>()=
INNWATCH_LIBSPACE	25000
##  Number of inodes at which to throttle innd on _PATH_SPOOL.
#### =()<INNWATCH_SPOOLNODES	@<INNWATCH_SPOOLNODES>@>()=
INNWATCH_SPOOLNODES	200
##  How long to sleep between innwatch iterations.
#### =()<INNWATCH_SLEEPTIME	@<INNWATCH_SLEEPTIME>@>()=
INNWATCH_SLEEPTIME	600
##  How inn (not just innwatch anymore) gets disk space usage 
##  (SVR4 machine would probably use /usr/ucb/df)
#### =()<INNWATCH_DF		@<INNWATCH_DF>@>()=
INNWATCH_DF		/bin/df
##  Field number of INNWATCH_DF (with -i) output that gives free
##  inodes (starting at 1).
#### =()<INNWATCH_INODES		@<INNWATCH_INODES>@>()=
INNWATCH_INODES		7
##  Field number of INNWATCH_DF output that gives the free block count.
##### =()<INNWATCH_BLOCKS		@<INNWATCH_BLOCKS>@>()=
INNWATCH_BLOCKS		4



##
##  13.  TCL Configuration
##  Do you want TCL support?  Pick DO or DONT
#### =()<TCL_SUPPORT		@<TCL_SUPPORT>@>()=
TCL_SUPPORT		DONT
##  Where your tcl header file live. Blank if no TCL
### =()<TCL_INC		@<TCL_INC>@>()=
TCL_INC		
# TCL_INC		-I/usr/local/include
##  How do you get the TCL library?  Probably -ltcl -lm, blank if no TCL
#### =()<TCL_LIB		@<TCL_LIB>@>()=
TCL_LIB		
# TCL_LIB		-ltcl -lm
##  TCL Startup File. Should probably be prefixed with the same value as
##  _PATH_CONTROLPROGS. 
#### =()<_PATH_TCL_STARTUP	@<_PATH_TCL_STARTUP>@>()=
_PATH_TCL_STARTUP	/usr/news/bin/control/startup.tcl
##  TCL Filter File. Should probably be prefixed with the same value as
##  _PATH_CONTROLPROGS.
#### =()<_PATH_TCL_FILTER	@<_PATH_TCL_FILTER>@>()=
_PATH_TCL_FILTER	/usr/news/bin/control/filter.tcl


##
##  14. PGP Configuration
##

## Do you want PGP verification of control messages? Pick DO or DONT
## If you change this then you need to fiddle with control.ctl.
## Setting it DONT is a really bad idea with all the miscreants out there...
#### =()<WANT_PGPVERIFY		@<WANT_PGPVERIFY>@>()=
WANT_PGPVERIFY		DO
##  PGP binary.
#### =()<_PATH_PGP		@<_PATH_PGP>@>()=
_PATH_PGP		/usr/local/bin/pgp

##
##  15. Local Configuration
##

##  Home of the NEWSUSER
#### =()<_PATH_NEWSHOME		@<_PATH_NEWSHOME>@>()=
_PATH_NEWSHOME		/usr/news


##
## 16. Actsync Configuration.
##

##  Host to synchoronize active file with (see actsync and actsyncd manpages).
#### =()<ACTSYNC_HOST		@<ACTSYNC_HOST>@>()=
ACTSYNC_HOST		news.foo.bar.com
##  Flags actsyncd is to pass to actsync.
#### =()<ACTSYNC_FLAGS		@<ACTSYNC_FLAGS>@>()=
ACTSYNC_FLAGS		-v 2 -q 2 -b 0 -d 0 -g 7 -s 79 -t 2
## Actsyncd config file.
#### =()<_PATH_ACTSYNC_CFG	@<_PATH_ACTSYNC_CFG>@>()=
_PATH_ACTSYNC_CFG	/var/news/etc/actsync.cfg
## Actsyncd ignore file.
#### =()<_PATH_ACTSYNC_IGN	@<_PATH_ACTSYNC_IGN>@>()=
_PATH_ACTSYNC_IGN	/var/news/etc/actsync.ign



##  17.  Perl Configuration
##  Do you want Perl support?  Pick DO or DONT
#### =()<PERL_SUPPORT		@<PERL_SUPPORT>@>()=
PERL_SUPPORT		DONT
##  How do you get the PERL libraries?  Probably -lperl -lm, blank if no PERL
#### =()<PERL_LIB	@<PERL_LIB>@>()=
PERL_LIB	
#PERL_LIB	-L/usr/local/gnu/lib/perl5/i386-bsdos/5.003/CORE -lperl -lm
##  Core directory (includes, libs). See perlembed(1)
#### =()<PERL_INC	@<PERL_INC>@>()=
PERL_INC	
#PERL_INC		-I/usr/local/gnu/lib/perl5/i386-bsdos/5.003/CORE
##  Perl Filter Files. Should probably be prefixed with the same value as
##  _PATH_CONTROLPROGS.
#### =()<_PATH_PERL_STARTUP_INND		@<_PATH_PERL_STARTUP_INND>@>()=
_PATH_PERL_STARTUP_INND		/usr/news/bin/control/startup_innd.pl
#### =()<_PATH_PERL_FILTER_INND		@<_PATH_PERL_FILTER_INND>@>()=
_PATH_PERL_FILTER_INND		/usr/news/bin/control/filter_innd.pl
#### =()<_PATH_PERL_FILTER_NNRPD		@<_PATH_PERL_FILTER_NNRPD>@>()=
_PATH_PERL_FILTER_NNRPD		/usr/news/bin/control/filter_nnrpd.pl
