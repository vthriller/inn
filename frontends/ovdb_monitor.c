/*
 * ovdb_monitor
 *   Performs database maintenance tasks
 *   + Transaction checkpoints
 *   + Deadlock detection
 *   + Transaction log removal
 */
#include "config.h"
#include "clibrary.h"
#include "libinn.h"
#include <syslog.h>
#include <string.h>
#include <signal.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(status)    ((((unsigned)(status)) >> 8) & 0xFF)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status)      ((((unsigned)(status)) & 0xFF) == 0)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(status)    ((((unsigned)(status)) & 0xFF) > 0 \
                                 && (((unsigned)(status)) & 0xFF00) == 0)
#endif

#ifndef USE_BERKELEY_DB

int main(int argc, char **argv)
{
    exit(0);
}

#else /* USE_BERKELEY_DB */

#include <db.h>
#include "ov.h"
#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"


#ifdef _HPUX_SOURCE
#include <sys/pstat.h>
#else
static char     *TITLEstart;
static char     *TITLEend;
#endif

static void TITLEset(char *what)
{
#if defined(HAVE_SETPROCTITLE)
    setproctitle("%s", what);
#else
#if     !defined(_HPUX_SOURCE)
    register char       *p;
    register int        i;
    char                buff[BUFSIZ];

    p = TITLEstart;
/*    *p++ = '-';*/

    strcpy(buff, what);
    i = strlen(buff);
    if (i > TITLEend - p - 2) {
        i = TITLEend - p - 2;
        buff[i] = '\0';
    }
    (void)strcpy(p, buff);
    for (p += i; p < TITLEend; )
        *p++ = ' ';
#else
    char                buff[BUFSIZ];
    union pstun un;

    un.pst_command = what;
    (void)pstat(PSTAT_SETCMD, un, strlen(buff), 0, 0);
#endif  /* !defined(_HPUX_SOURCE) */
#endif  /* defined(HAVE_SETPROCTITLE) */
}

static int signalled = 0;
static void sigfunc(int sig)
{
    signalled = 1;
}


static pid_t deadlockpid = 0;
static pid_t checkpointpid = 0;
static pid_t logremoverpid = 0;

static int putpid(char *path)
{
    char buf[30];
    int fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, 0664);
    if(!fd) {
	syslog(L_FATAL, "can't open %s: %m", path);
	return -1;
    }
    sprintf(buf, "%d", getpid());
    if(write(fd, buf, strlen(buf)) < 0) {
	syslog(L_FATAL, "can't write to %s: %m", path);
	close(fd);
	return -1;
    }
    close(fd);
    return 0;
}

static void deadlock(void)
{
    int ret, status = 0;
    u_int32_t atype = DB_LOCK_YOUNGEST;

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    TITLEset("ovdb_monitor: deadlock");

    while(!signalled) {
#if DB_VERSION_MAJOR == 2
	ret = lock_detect(OVDBenv->lk_info, 0, atype);
#else
	ret = lock_detect(OVDBenv, 0, atype, NULL);
#endif
	if(ret != 0) {
	    syslog(L_ERROR, "OVDB: lock_detect: %s", db_strerror(ret));
	    status = 1;
	    break;
	}
	sleep(30);
    }

    ovdb_close_berkeleydb();
    _exit(status);
}

static void checkpoint(void)
{
    int ret, status = 0;

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    TITLEset("ovdb_monitor: checkpoint");

    while(!signalled) {
#if DB_VERSION_MAJOR == 2
	ret = txn_checkpoint(OVDBenv->tx_info, 2048, 1);
#else
#if DB_VERSION_MINOR == 0
	ret = txn_checkpoint(OVDBenv, 2048, 1);
#else
	ret = txn_checkpoint(OVDBenv, 2048, 1, 0);
#endif
#endif
	if(ret != 0 && ret != DB_INCOMPLETE) {
	    syslog(L_ERROR, "OVDB: txn_checkpoint: %s", db_strerror(ret));
	    status = 1;
	    break;
	}
	if(ret == DB_INCOMPLETE)
	    sleep(2);
	else
	    sleep(30);
    }

    ovdb_close_berkeleydb();
    _exit(status);
}

static void logremover(void)
{
    int ret, status = 0;
    char **listp, **p;

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    TITLEset("ovdb_monitor: logremover");

    while(!signalled) {
#if DB_VERSION_MAJOR == 2
	ret = log_archive(OVDBenv->lg_info, &listp, DB_ARCH_ABS, malloc);
#else
	ret = log_archive(OVDBenv, &listp, DB_ARCH_ABS, malloc);
#endif
	if(ret != 0) {
	    syslog(L_ERROR, "OVDB: log_archive: %s", db_strerror(ret));
	    status = 1;
	    break;
	}
	if(listp != NULL) {
	    for(p = listp; *p; p++)
		unlink(*p);
	    free(listp);
	}
	sleep(45);
    }

    ovdb_close_berkeleydb();
    _exit(status);
}

static int start_process(pid_t *pid, void (*func)(void))
{
    pid_t child;

    switch(child = fork()) {
    case 0:
	(*func)();
	_exit(0);
    case -1:
	syslog(L_FATAL, "can't fork: %m");
	return -1;
    default:
	*pid = child;
	return 0;
    }
    /*NOTREACHED*/
}

static void cleanup(int status)
{
    int cs;

    if(deadlockpid)
	kill(deadlockpid, SIGTERM);
    if(checkpointpid)
	kill(checkpointpid, SIGTERM);
    if(logremoverpid)
	kill(logremoverpid, SIGTERM);

    xsignal(SIGINT, SIG_DFL);
    xsignal(SIGTERM, SIG_DFL);
    xsignal(SIGHUP, SIG_DFL);

    if(deadlockpid)
	waitpid(deadlockpid, &cs, 0);
    if(checkpointpid)
	waitpid(checkpointpid, &cs, 0);
    if(logremoverpid)
	waitpid(logremoverpid, &cs, 0);

    unlink(cpcatpath(innconf->pathrun, OVDB_MONITOR_PIDFILE));
    exit(status);
}

static int monitorloop(void)
{
    int cs, restartit;
    pid_t child;

    while(!signalled) {
	child = waitpid(-1, &cs, WNOHANG);
	if(child > 0) {
	    if(WIFSIGNALED(cs)) {
		restartit = 0;
	    } else {
		if(WEXITSTATUS(cs) == 0)
		    restartit = 1;
		else
		    restartit = 0;
	    }
	    if(child == deadlockpid) {
		deadlockpid = 0;
		if(restartit && start_process(&deadlockpid, deadlock))
		    cleanup(1);
	    } else if(child == checkpointpid) {
		checkpointpid = 0;
		if(restartit && start_process(&checkpointpid, checkpoint))
		    cleanup(1);
	    } else if(child == logremoverpid) {
		logremoverpid = 0;
		if(restartit && start_process(&logremoverpid, logremover))
		    cleanup(1);
	    }
	    if(!restartit)
		cleanup(1);
	}
	sleep(20);
    }
    cleanup(0);
}


int main(int argc, char **argv)
{
    if(argc != 2 || strcmp(argv[1], SPACES)) {
	fprintf(stderr, "Use ovdb_init to start me\n");
	exit(1);
    }
#if     !defined(_HPUX_SOURCE) && !defined(HAVE_SETPROCTITLE)
    /* Save start and extent of argv for TITLEset. */
    TITLEstart = argv[0];
    TITLEend = argv[argc - 1] + strlen(argv[argc - 1]) - 1;
#endif

    openlog("ovdb_monitor", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if (ReadInnConf() < 0)
	exit(1);

    if(strcmp(innconf->ovmethod, "ovdb")) {
	syslog(L_FATAL, "ovmethod not set to ovdb");
        exit(1);
    }
    if(!ovdb_getlock(OVDB_LOCK_ADMIN)) {
	syslog(L_FATAL, "can't lock database");
	exit(1);
    }

    xsignal(SIGINT, sigfunc);
    xsignal(SIGTERM, sigfunc);
    xsignal(SIGHUP, sigfunc);

    if(putpid(cpcatpath(innconf->pathrun, OVDB_MONITOR_PIDFILE)))
	exit(1);
    if(start_process(&deadlockpid, deadlock))
	cleanup(1);
    if(start_process(&checkpointpid, checkpoint))
	cleanup(1);
    if(start_process(&logremoverpid, logremover))
	cleanup(1);

    monitorloop();
    /*NOTREACHED*/
}

#endif /* USE_BERKELEY_DB */

