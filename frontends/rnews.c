/*  $Id$
**
**  A front-end for InterNetNews.
**  Read UUCP batches and offer them up NNTP-style.  Because we may end
**  up sending our input down a pipe to uncompress, we have to be careful
**  to do unbuffered reads.
*/
#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"


typedef struct _HEADER {
    STRING	Name;
    int		size;
} HEADER;


STATIC BOOL	Verbose;
STATIC char	*InputFile = "stdin";
STATIC char	*UUCPHost;
STATIC char	*PathBadNews = NULL;
STATIC char	*remoteServer;
STATIC FILE	*FromServer;
STATIC FILE	*ToServer;
STATIC char	UNPACK[] = "gzip";
STATIC HEADER	RequiredHeaders[] = {
    { "Message-ID",	10 },
#define _messageid	0
    { "Newsgroups",	10 },
#define _newsgroups	1
    { "From",		 4 },
#define _from		2
    { "Date",		 4 },
#define _date		3
    { "Subject",	 7 },
#define _subject	4
    { "Path",		 4 },
#define _path		5
};
#define IS_MESGID(hp)	((hp) == &RequiredHeaders[_messageid])
#define IS_PATH(hp)	((hp) == &RequiredHeaders[_path])



/*
**  Do perror, making sure errno is preserved.
*/
STATIC void
xperror(p)
    char	*p;
{
    int		oerrno;

    oerrno = errno;
    perror(p);
    errno = oerrno;
}


/*
**  Open up a pipe to a process with fd tied to its stdin.  Return a
**  descriptor tied to its stdout or -1 on error.
*/
STATIC int
StartChild(fd, path, argv)
    int		fd;
    char	*path;
    char	*argv[];
{
    int		pan[2];
    int		i;
    PID_T	pid;

    /* Create a pipe. */
    if (pipe(pan) < 0) {
	syslog(L_FATAL, "cant pipe for %s %m", path);
	exit(1);
    }

    /* Get a child. */
    for (i = 0; (pid = FORK()) < 0; i++) {
	if (i == innconf->maxforks) {
	    syslog(L_ERROR, "cant fork %s %m -- spooling", path);
	    return -1;
	}
	syslog(L_NOTICE, "cant fork %s -- waiting", path);
	(void)sleep(60);
    }

    /* Run the child, with redirection. */
    if (pid == 0) {
	(void)close(pan[PIPE_READ]);

	/* Stdin comes from our old input. */
	if (fd != STDIN) {
	    if ((i = dup2(fd, STDIN)) != STDIN) {
		syslog(L_FATAL, "cant dup2 %d to 0 got %d %m", fd, i);
		_exit(1);
	    }
	    (void)close(fd);
	}

	/* Stdout goes down the pipe. */
	if (pan[PIPE_WRITE] != STDOUT) {
	    if ((i = dup2(pan[PIPE_WRITE], STDOUT)) != STDOUT) {
		syslog(L_FATAL, "cant dup2 %d to 1 got %d %m",
		    pan[PIPE_WRITE], i);
		_exit(1);
	    }
	    (void)close(pan[PIPE_WRITE]);
	}

	(void)execv(path, argv);
	syslog(L_FATAL, "cant execv %s %m", path);
	_exit(1);
    }

    (void)close(pan[PIPE_WRITE]);
    (void)close(fd);
    return pan[PIPE_READ];
}


/*
**  Wait for the specified number of children.
*/
STATIC void
WaitForChildren(i)
    register int	i;
{
    register PID_T	pid;
    int			status;

    while (--i >= 0) {
	pid = waitnb(&status);
	if (pid < 0) {
	    if (errno != ECHILD)
		syslog(L_ERROR, "cant wait %m");
	    break;
	}
    }
}




/*
**  Clean up the NNTP escapes from a line.
*/
STATIC char *REMclean(char *buff)
{
    char	*p;

    if ((p = strchr(buff, '\r')) != NULL)
	*p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';

    /* The dot-escape is only in text, not command responses. */
    return buff;
}


/*
**  Write an article to the rejected directory.
*/
STATIC void Reject(const char *article, const char *reason, const char *arg)
{
#if	defined(DO_RNEWS_SAVE_BAD)
    char	buff[SMBUF];
    FILE	*F;
    int		i;
#endif	/* defined(DO_RNEWS_SAVE_BAD) */

    syslog(L_NOTICE, reason, arg);
    if (Verbose) {
	(void)fprintf(stderr, "%s: ", InputFile);
	(void)fprintf(stderr, reason, arg);
	(void)fprintf(stderr, " [%.40s...]\n", article);
    }

#if	defined(DO_RNEWS_SAVE_BAD)
    TempName(PathBadNews, buff);
    if ((F = fopen(buff, "w")) == NULL) {
	syslog(L_ERROR, "cant fopen %s %m", buff);
	return;
    }
    i = strlen(article);
    if (fwrite((POINTER)article, (SIZE_T)1, (SIZE_T)i, F) != i)
	syslog(L_ERROR, "cant fwrite %s %m", buff);
    if (fclose(F) == EOF)
	syslog(L_ERROR, "cant close %s %m", buff);
#endif	/* defined(DO_RNEWS_SAVE_BAD) */
}


/*
**  Process one article.  Return TRUE if the article was okay; FALSE if the
**  whole batch needs to be saved (such as when the server goes down or if
**  the file is corrupted).
*/
STATIC BOOL Process(char *article)
{
    HEADER	        *hp;
    char	        *p;
    char		*id;
    char		buff[SMBUF];
#if	defined(FILE_RNEWS_LOG_DUPS)
    FILE		*F;
#endif	/* defined(FILE_RNEWS_LOG_DUPS) */
#if	!defined(DONT_RNEWS_LOG_DUPS)
    char		path[40];
#endif	/* !defined(DONT_RNEWS_LOG_DUPS) */

    /* Empty article? */
    if (*article == '\0')
	return TRUE;

    /* Make sure that all the headers are there, note the ID. */
    for (hp = RequiredHeaders; hp < ENDOF(RequiredHeaders); hp++) {
	if ((p = (char *)HeaderFindMem(article, strlen(article), hp->Name, hp->size)) == NULL) {
	    Reject(article, "bad_article missing %s", hp->Name);
	    return FALSE;
	}
	if (IS_MESGID(hp)) {
	    id = p;
	    continue;
	}
#if	!defined(DONT_RNEWS_LOG_DUPS)
	if (IS_PATH(hp)) {
	    (void)strncpy(path, p, sizeof path);
	    path[sizeof path - 1] = '\0';
	    if ((p = strchr(path, '\n')) != NULL)
		*p = '\0';
	}
#endif	/* !defined(DONT_RNEWS_LOG_DUPS) */
    }

    /* Send the NNTP "ihave" message. */
    if ((p = strchr(id, '\n')) == NULL) {
	Reject(article, "bad_article unterminated %s header", "Message-ID");
	return FALSE;
    }
    *p = '\0';
    (void)fprintf(ToServer, "ihave %s\r\n", id);
    (void)fflush(ToServer);
    if (UUCPHost)
	syslog(L_NOTICE, "offered %s %s", id, UUCPHost);
    *p = '\n';

    /* Get a reply, see if they want the article. */
    if (fgets(buff, sizeof buff, FromServer) == NULL) {
	syslog(L_ERROR, "cant fgets after ihave %m");
	return FALSE;
    }
    (void)REMclean(buff);
    if (!CTYPE(isdigit, buff[0])) {
	syslog(L_NOTICE, "bad_reply after ihave %s", buff);
	return FALSE;
    }
    switch (atoi(buff)) {
    default:
	Reject(article, "unknown_reply after ihave %s", buff);
	return TRUE;
    case NNTP_RESENDIT_VAL:
	return FALSE;
    case NNTP_SENDIT_VAL:
	break;
    case NNTP_HAVEIT_VAL:
#if	defined(SYSLOG_RNEWS_LOG_DUPS)
	*p = '\0';
	syslog(L_NOTICE, "duplicate %s %s", id, path);
#endif	/* defined(SYSLOG_RNEWS_LOG_DUPS) */
#if	defined(FILE_RNEWS_LOG_DUPS)
	if ((F = fopen(_PATH_RNEWS_DUP_LOG, "a")) != NULL) {
	    *p = '\0';
	    (void)fprintf(F, "duplicate %s %s\n", id, path);
	    (void)fclose(F);
	}
#endif	/* defined(FILE_RNEWS_LOG_DUPS) */
	return TRUE;
    }

    /* Send all the lines in the article, escaping periods. */
    if (NNTPsendarticle(article, ToServer, TRUE) < 0) {
	syslog(L_NOTICE, "cant sendarticle %m");
	return FALSE;
    }

    /* Process server reply code. */
    if (fgets(buff, sizeof buff, FromServer) == NULL) {
	syslog(L_ERROR, "cant fgets after article %m");
	return FALSE;
    }
    (void)REMclean(buff);
    if (!CTYPE(isdigit, buff[0])) {
	syslog(L_NOTICE, "bad_reply after article %s", buff);
	return FALSE;
    }
    switch (atoi(buff)) {
    default:
	syslog(L_NOTICE, "unknown_reply after article %s", buff);
	/* FALLTHROUGH */
    case NNTP_RESENDIT_VAL:
	return FALSE;
    case NNTP_TOOKIT_VAL:
	break;
    case NNTP_REJECTIT_VAL:
	Reject(article, "rejected %s", buff);
	break;
    }
    return TRUE;
}


/*
**  Read the rest of the input as an article.  Just punt to stdio in
**  this case and let it do the buffering.
*/
STATIC BOOL
ReadRemainder(fd, first, second)
    register int	fd;
    char		first;
    char		second;
{
    register FILE	*F;
    register char	*article;
    register int	size;
    register int	used;
    register int	left;
    register int	i;
    BOOL		ok;

    /* Turn the descriptor into a stream. */
    if ((F = fdopen(fd, "r")) == NULL) {
	syslog(L_FATAL, "can't fdopen %d %m", fd);
	exit(1);
    }

    /* Get an initial allocation, leaving space for the \0. */
    size = BUFSIZ + 1;
    article = NEW(char, size + 2);
    article[0] = first;
    article[1] = second;
    used = second ? 2 : 1;
    left = size - used;

    /* Read the input. */
    while ((i = fread((POINTER)&article[used], (SIZE_T)1,
		    (SIZE_T)left, F)) != 0) {
	if (i < 0) {
	    syslog(L_FATAL, "cant fread after %d bytes %m", used);
	    exit(1);
	}
	used += i;
	left -= i;
	if (left < SMBUF) {
	    size += BUFSIZ;
	    left += BUFSIZ;
	    RENEW(article, char, size);
	}
    }
    if (article[used - 1] != '\n')
	article[used++] = '\n';
    article[used] = '\0';
    (void)fclose(F);

    ok = Process(article);
    DISPOSE(article);
    return ok;
}


/*
**  Read an article from the input stream that is artsize bytes long.
*/
STATIC BOOL
ReadBytecount(fd, artsize)
    register int	fd;
    int			artsize;
{
    static char		*article;
    static int		oldsize;
    register char	*p;
    register int	left;
    register int	i;

    /* If we haven't gotten any memory before, or we didn't get enough,
     * then get some. */
    if (article == NULL) {
	oldsize = artsize;
	article = NEW(char, oldsize + 1 + 1);
    }
    else if (artsize > oldsize) {
	oldsize = artsize;
	RENEW(article, char, oldsize + 1 + 1);
    }

    /* Read in the article. */
    for (p = article, left = artsize; left; p += i, left -= i)
	if ((i = read(fd, p, left)) <= 0) {
	    i = errno;
	    syslog(L_ERROR, "cant read wanted %d got %d %m",
		artsize, artsize - left);
#if	0
	    /* Don't do this -- if the article gets re-processed we
	     * will end up accepting the truncated version. */
	    artsize = p - article;
	    article[artsize] = '\0';
	    Reject(article, "short read (%s?)", strerror(i));
#endif	/* 0 */
	    return TRUE;
	}
    if (p[-1] != '\n')
	*p++ = '\n';
    *p = '\0';

    return Process(article);
}



/*
**  Read a single text line; not unlike fgets().  Just more inefficient.
*/
STATIC BOOL
ReadLine(p, size, fd)
    char	*p;
    int		size;
    int		fd;
{
    char	*save;

    /* Fill the buffer, a byte at a time. */
    for (save = p; size > 0; p++, size--) {
	if (read(fd, p, 1) != 1) {
	    *p = '\0';
	    syslog(L_FATAL, "cant read first line got %s %m", save);
	    exit(1);
	}
	if (*p == '\n') {
	    *p = '\0';
	    return TRUE;
	}
    }
    *p = '\0';
    syslog(L_FATAL, "bad_line too long %s", save);
    return FALSE;
}


/*
**  Unpack a single batch.
*/
STATIC BOOL
UnpackOne(fdp, countp)
    int		*fdp;
    int		*countp;
{
#if	defined(DO_RNEWSPROGS)
    char	path[(SMBUF * 2) + 1];
    char	*p;
#endif	/* defined(DO_RNEWSPROGS) */
    char	buff[SMBUF];
    STRING	cargv[4];
    int		artsize;
    int		i;
    int		gzip = 0;
    BOOL	HadCount;
    BOOL	SawCunbatch;
    int		len;

    *countp = 0;
    for (SawCunbatch = FALSE, HadCount = FALSE; ; ) {
	/* Get the first character. */
	if ((i = read(*fdp, &buff[0], 1)) < 0) {
	    syslog(L_ERROR, "cant read first character %m");
	    return FALSE;
	}
	if (i == 0)
	    break;

	if (buff[0] == 0x1f)
	    gzip = 1;
	else if (buff[0] != RNEWS_MAGIC1)
	    /* Not a batch file.  If we already got one count, the batch
	     * is corrupted, else read rest of input as an article. */
	    return HadCount ? FALSE : ReadRemainder(*fdp, buff[0], '\0');

	/* Get the second character. */
	if ((i = read(*fdp, &buff[1], 1)) < 0) {
	    syslog(L_ERROR, "cant read second character %m");
	    return FALSE;
	}
	if (i == 0)
	    /* A one-byte batch? */
	    return FALSE;

	/* Check second magic character. */
	/* gzipped ($1f$8b) or compressed ($1f$9d) */
	if (gzip && ((buff[1] == (char)0x8b) || (buff[1] == (char)0x9d))) {
	    cargv[0] = "gzip";
	    cargv[1] = "-d";
	    cargv[2] = NULL;
	    lseek(*fdp, (OFFSET_T) 0, 0); /* Back to the beginning */
	    *fdp = StartChild(*fdp, _PATH_GZIP, cargv);
	    if (*fdp < 0)
	        return FALSE;
	    (*countp)++;
	    SawCunbatch = TRUE;
	    continue;
	}
	if (buff[1] != RNEWS_MAGIC2)
	    return HadCount ? FALSE : ReadRemainder(*fdp, buff[0], buff[1]);

	/* Some kind of batch -- get the command. */
	if (!ReadLine(&buff[2], (int)(sizeof buff - 3), *fdp))
	    return FALSE;

	if (strncmp(buff, "#! rnews ", 9) == 0) {
	    artsize = atoi(&buff[9]);
	    if (artsize <= 0) {
		syslog(L_ERROR, "bad_line bad count %s", buff);
		return FALSE;
	    }
	    HadCount = TRUE;
	    if (ReadBytecount(*fdp, artsize))
		continue;
	    return FALSE;
	}

	if (HadCount)
	    /* Already saw a bytecount -- probably corrupted. */
	    return FALSE;

	if (strcmp(buff, "#! cunbatch") == 0) {
	    if (SawCunbatch) {
		syslog(L_ERROR, "nested_cunbatch");
		return FALSE;
	    }
	    cargv[0] = UNPACK;
	    cargv[1] = "-d";
	    cargv[2] = NULL;
	    *fdp = StartChild(*fdp, _PATH_GZIP, cargv);
	    if (*fdp < 0)
		return FALSE;
	    (*countp)++;
	    SawCunbatch = TRUE;
	    continue;
	}

#if	defined(DO_RNEWSPROGS)
	cargv[0] = UNPACK;
	cargv[1] = NULL;
	/* Ignore any possible leading pathnames, to avoid trouble. */
	if ((p = strrchr(&buff[3], '/')) != NULL)
	    p++;
	else
	    p = &buff[3];
	if (strchr(_PATH_RNEWSPROGS, '/') == NULL) {
	    (void)sprintf(path, "%s/%s/%s", innconf->pathbin,
					_PATH_RNEWSPROGS, p);
	    len = strlen(innconf->pathbin) + 1 + sizeof _PATH_RNEWSPROGS;
	} else {
	    (void)sprintf(path, "%s/%s", _PATH_RNEWSPROGS, p);
	    len = sizeof _PATH_RNEWSPROGS;
	}
	for (p = &path[len]; *p; p++)
	    if (ISWHITE(*p)) {
		*p = '\0';
		break;
	    }
	*fdp = StartChild(*fdp, path, cargv);
	if (*fdp < 0)
	    return FALSE;
	(*countp)++;
	continue;
#else
	syslog(L_ERROR, "bad_format unknown command %s", buff);
	return FALSE;
#endif	/* defined(DO_RNEWSPROGS) */
    }
    return TRUE;
}


/*
**  Read all articles in the spool directory and unpack them.  Print all
**  errors with xperror as well as syslog, since we're probably being run
**  interactively.
*/
STATIC void
Unspool()
{
    register DIR	*dp;
    struct dirent       *ep;
    register BOOL	ok;
    struct stat		Sb;
    char		buff[SMBUF];
    char		hostname[10];
    int			fd;
    int			i;

    /* Go to the spool directory, get ready to scan it. */
    if (chdir(innconf->pathincoming) < 0) {
	xperror(innconf->pathincoming);
	syslog(L_FATAL, "cant cd %s %m", innconf->pathincoming);
	exit(1);
    }
    if ((dp = opendir(".")) == NULL) {
	xperror("Can't open spool directory");
	syslog(L_FATAL, "cant opendir . %m");
	exit(1);
    }

    /* Loop over all files, and parse them. */
    while ((ep = readdir(dp)) != NULL) {
	InputFile = ep->d_name;
	if (InputFile[0] == '.')
	    continue;
	if (stat(InputFile, &Sb) < 0 && errno != ENOENT) {
	    xperror(InputFile);
	    syslog(L_ERROR, "cant stat %s %m", InputFile);
	    continue;
	}

	if (!S_ISREG(Sb.st_mode))
	    continue;

	if ((fd = open(InputFile, O_RDONLY)) < 0) {
	    if (errno != ENOENT) {
		xperror(InputFile);
		syslog(L_ERROR, "cant open %s %m", InputFile);
	    }
	    continue;
	}
	/* Get UUCP host from spool file, deleting the mktemp XXXXXX suffix. */
	if ((i = strlen(InputFile)) > 6)
	    i -= 6;
	if (i > sizeof hostname - 1)
	    /* Just in case someone wrote their own spooled file. */
	    i = sizeof hostname - 1;
	(void)strncpy(hostname, InputFile, i);
	hostname[i] = '\0';
	UUCPHost = hostname;
	ok = UnpackOne(&fd, &i);
	(void)close(fd);
	WaitForChildren(i);

	if (!ok) {
	    TempName(PathBadNews, buff);
	    (void)fprintf(stderr, "Unspooling failed saving to %s\n", buff);
	    syslog(L_ERROR, "cant unspool saving to %s", buff);
	    if (rename(InputFile, buff) < 0) {
		xperror(buff);
		syslog(L_FATAL, "cant rename %s to %s %m", InputFile, buff);
		exit(1);
	    }
	    continue;
	}

	if (unlink(InputFile) < 0)
	    syslog(L_ERROR, "cant remove %s %m", InputFile);
    }
    (void)closedir(dp);
}



/*
**  Can't connect to the server, so spool our input.  There isn't much
**  we can do if this routine fails, unfortunately.  Perhaps try to use
**  an alternate filesystem?
*/
STATIC void
Spool(fd, mode)
    register int	fd;
    int			mode;
{
    register int	spfd;
    register int	i;
    register int	j;
    register char	*p;
    char		temp[BUFSIZ];
    char		buff[BUFSIZ];
    int			count;
    int			status;

    if(mode == 'N')
	exit(9);
    TempName(innconf->pathtmp, temp);
    (void)umask(0);
    if ((spfd = open(temp, O_WRONLY | O_CREAT, BATCHFILE_MODE)) < 0) {
	syslog(L_FATAL, "cant open %s %m", temp);
	exit(1);
    }

    /* Read until we there is nothing left. */
    for (status = 0, count = 0; (i = read(fd, buff, sizeof buff)) != 0; ) {
	/* Break out on error. */
	if (i < 0) {
	    syslog(L_FATAL, "cant read after %d %m", count);
	    status++;
	    break;
	}
	/* Write out what we read. */
	for (count += i, p = buff; i; p += j, i -= j)
	    if ((j = write(spfd, (POINTER)p, (SIZE_T)i)) <= 0) {
		syslog(L_FATAL, "cant write around %d %m", count);
		status++;
		break;
	    }
    }

    /* Close the file. */
    if (close(spfd) < 0) {
	syslog(L_FATAL, "cant close spooled rnews %m");
	status++;
    }

    /* Move temp file into the spool area, and exit appropriately. */
    TempName(innconf->pathincoming, buff);
    if (rename(temp, buff) < 0) {
	syslog(L_FATAL, "cant rename %s to %s %m", temp, buff);
	status++;
    }
    exit(status);
    /* NOTREACHED */
}


/*
**  Try to read the password file and open a connection to a remote
**  NNTP server.
*/
STATIC BOOL OpenRemote(char *server, int port, char *buff)
{
    int		i;

    /* Open the remote connection. */
    if (server)
	i = NNTPconnect(server, port, &FromServer, &ToServer, buff);
    else
	i = NNTPremoteopen(port, &FromServer, &ToServer, buff);
    if (i < 0)
	return FALSE;

    *buff = '\0';
    if (NNTPsendpassword((char *)NULL, FromServer, ToServer) < 0) {
	int oerrno = errno;
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	errno = oerrno;
	return FALSE;
    }
    return TRUE;
}


/*
**  Can't connect to server; print message and spool if necessary.
*/
STATIC NORETURN
CantConnect(buff, mode, fd)
    char	*buff;
    int		mode;
    int		fd;
{
    if (buff[0])
	syslog(L_NOTICE, "rejected connection %s", REMclean(buff));
    else
	syslog(L_FATAL, "cant open_remote %m");
    if (mode != 'U')
	Spool(fd, mode);
    exit(1);
}


/*
**  Log an incorrect usage.
*/
STATIC NORETURN
Usage()
{
    syslog(L_FATAL, "usage error");
    exit(1);
}


int main(int ac, char *av[])
{
    int		fd;
    int		i;
    int		mode;
    char	buff[SMBUF];
    int         port = NNTP_PORT;

    /* First thing, set up logging and our identity. */
    openlog("rnews", L_OPENLOG_FLAGS, LOG_INN_PROG);
#if !defined(__FreeBSD__) && !defined(__bsdi__) && !defined (__NetBSD__)
    if (setgid(getegid()) < 0) {
	syslog(L_FATAL, "cant setgid to %d %m", getegid());
	exit(1);
    }
    if (setuid(geteuid()) < 0) {
	syslog(L_FATAL, "cant setuid to %d %m", geteuid());
	exit(1);
    }
#endif
     if (ReadInnConf() < 0) exit(1);
     UUCPHost = getenv(_ENV_UUCPHOST);
     PathBadNews = COPY(cpcatpath(innconf->pathincoming, _PATH_BADNEWS));
     port = innconf->nnrpdpostport;

    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    fd = STDIN;
    mode = '\0';
    while ((i = getopt(ac, av, "h:P:NUvr:S:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTRTEACHED */
	case 'h':
	    UUCPHost = *optarg ? optarg : NULL;
	    break;
	case 'N':
	case 'U':
	    mode = i;
	    break;
	case 'P':
	    port = atoi(optarg);
	    break;
	case 'v':
	    Verbose = TRUE;
	    break;
	case 'r':
	case 'S':
	    remoteServer = optarg;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Parse arguments.  At most one, the input file. */
    switch (ac) {
    default:
	Usage();
	/* NOTREACHED */
    case 0:
	break;
    case 1:
	if (mode == 'U')
	    Usage();
	if (freopen(av[0], "r", stdin) == NULL) {
	    syslog(L_FATAL, "cant freopen %s %m", av[0]);
	    exit(1);
	}
	fd = fileno(stdin);
	InputFile = av[0];
	break;
    }

    /* Open the link to the server. */
    if (remoteServer != NULL) {
	if (!OpenRemote(remoteServer,port,buff))
		CantConnect(buff,mode,fd);
    } else if (innconf->nnrpdposthost != NULL) {
	if (!OpenRemote(innconf->nnrpdposthost,
	    (port != NNTP_PORT) ? port : innconf->nnrpdpostport, buff))
		CantConnect(buff, mode, fd);
    }
    else {
#if	defined(DO_RNEWSLOCALCONNECT)
	if (NNTPlocalopen(&FromServer, &ToServer, buff) < 0) {
	    /* If server rejected us, no point in continuing. */
	    if (buff[0])
		CantConnect(buff, mode, fd);
	    if (!OpenRemote((char *)NULL,
	    	(port != NNTP_PORT) ? port : innconf->port, buff))
			CantConnect(buff, mode, fd);
	}
#else
	if (!OpenRemote((char *)NULL, 
	    (port != NNTP_PORT) ? port : innconf->port, buff))
		CantConnect(buff, mode, fd);
#endif	/* defined(DO_RNEWSLOCALCONNECT) */
    }
    CloseOnExec((int)fileno(FromServer), TRUE);
    CloseOnExec((int)fileno(ToServer), TRUE);

    /* Execute the command. */
    if (mode == 'U')
	Unspool();
    else {
	if (!UnpackOne(&fd, &i))
	    Spool(fd, mode);
	WaitForChildren(i);
    }

    /* Tell the server we're quitting, get his okay message. */
    (void)fprintf(ToServer, "quit\r\n");
    (void)fflush(ToServer);
    (void)fgets(buff, sizeof buff, FromServer);

    /* Return the appropriate status. */
    exit(0);
    /* NOTREACHED */
}
