/*  $Revision$
**
**  Routines for the NNTP channel.  Other channels get the descriptors which
**  we turn into NNTP channels, and over which we speak NNTP.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "dbz.h"

#define BAD_COMMAND_COUNT	10
#define SAVE_AMT		10
#define ART_EOF(c, s)		\
    ((c) >= 5 && (s)[-5] == '\r' && (s)[-4] == '\n' && (s)[-3] == '.' \
     && (s)[-2] == '\r' && (s)[-1] == '\n')


/*
**  An entry in the dispatch table.  The name, and implementing function,
**  of every command we support.
*/
typedef struct _NCDISPATCH {
    STRING	Name;
    FUNCPTR	Function;
    int		Size;
} NCDISPATCH;

#if	0
static FUNCTYPE	NCarticle();
#endif	/* 0 */
static FUNCTYPE	NCauthinfo();
static FUNCTYPE	NChead();
static FUNCTYPE	NChelp();
static FUNCTYPE	NCihave();
static FUNCTYPE	NClist();
static FUNCTYPE	NCmode();
static FUNCTYPE	NCquit();
static FUNCTYPE	NCstat();
static FUNCTYPE	NCxpath();
static FUNCTYPE	NCxreplic();
static FUNCTYPE	NC_unimp();
/* new modules for streaming */
static FUNCTYPE	NCxbatch();
static FUNCTYPE	NCcheck();
static FUNCTYPE	NCtakethis();

STATIC int		NCcount;	/* Number of open connections	*/
STATIC NCDISPATCH	NCcommands[] = {
#if	0
    {	"article",	NCarticle },
#else
    {	"article",	NC_unimp },
#endif	/* 0 */
    {	"authinfo",	NCauthinfo },
    {	"help",		NChelp	},
    {	"ihave",	NCihave	},
    {	"check",	NCcheck	},
    {	"takethis",	NCtakethis },
    {	"list",		NClist	},
    {	"mode",		NCmode	},
    {	"quit",		NCquit	},
    {	"head",		NChead	},
    {	"stat",		NCstat	},
    {	"body",		NC_unimp },
    {	"group",	NC_unimp },
    {	"last",		NC_unimp },
    {	"newgroups",	NC_unimp },
    {	"newnews",	NC_unimp },
    {	"next",		NC_unimp },
    {	"post",		NC_unimp },
    {	"slave",	NC_unimp },
    {	"xbatch",	NCxbatch },
    {	"xhdr",		NC_unimp },
    {	"xpath",	NCxpath	},
    {	"xreplic",	NCxreplic }
};
STATIC char		*NCquietlist[] = {
    INND_QUIET_BADLIST
};
STATIC char		NCterm[] = "\r\n";
STATIC char		NCdotterm[] = ".\r\n";
STATIC char		NCbadcommand[] = NNTP_BAD_COMMAND;
STATIC STRING		NCgreeting;

/*
** Clear the WIP entry for the given channel
*/
STATIC void NCclearwip(CHANNEL *cp) {
    WIPfree(WIPbyhash(cp->CurrentMessageIDHash));
    HashClear(&cp->CurrentMessageIDHash);
}

/*
**  Write an NNTP reply message.
*/
STATIC void
NCwritetext(cp, text)
    CHANNEL	*cp;
    char	*text;
{
    RCHANremove(cp);
    WCHANset(cp, text, (int)strlen(text));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANadd(cp);
    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s > %s", CHANname(cp), text);
}


/*
**  Write an NNTP reply message.
**  Call only when we will stay in NCreader mode.
**  Tries to do the actual write if it will not block.
*/
STATIC void
NCwritereply(cp, text)
    CHANNEL	*cp;
    char	*text;
{
    register BUFFER	*bp;
    register int	i;

    bp = &cp->Out;
    i = bp->Left;
    WCHANappend(cp, text, (int)strlen(text));	/* text in buffer */
    WCHANappend(cp, NCterm, STRLEN(NCterm));	/* add CR NL to text */
    if (i == 0) {	/* if only data then try to write directly */
	i = write(cp->fd, &bp->Data[bp->Used], bp->Left);
	if (Tracing || cp->Tracing)
	    syslog(L_TRACE, "%s NCwritereply %d=write(%d, \"%.15s\", %d)",
		CHANname(cp), i, cp->fd,  &bp->Data[bp->Used], bp->Left);
	if (i > 0) bp->Used += i;
	if (bp->Used == bp->Left) bp->Used = bp->Left = 0;
	else i = 0;
    } else i = 0;
    if (i <= 0) {	/* write failed, queue it for later */
	RCHANremove(cp);
	WCHANadd(cp);
    }
    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s > %s", CHANname(cp), text);
}

/*
**  Tell the NNTP channel to go away.
*/
STATIC void
NCwriteshutdown(cp, text)
    CHANNEL	*cp;
    char	*text;
{
    RCHANremove(cp);
    WCHANset(cp, NNTP_GOODBYE, STRLEN(NNTP_GOODBYE));
    WCHANappend(cp, " ", 1);
    WCHANappend(cp, text, (int)strlen(text));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANadd(cp);
    cp->State = CSwritegoodbye;
}


/*
**  If a Message-ID is bad, write a reject message and return TRUE.
*/
STATIC BOOL
NCbadid(cp, p)
    register CHANNEL	*cp;
    register char	*p;
{
    if (ARTidok(p))
	return FALSE;

    NCwritetext(cp, NNTP_HAVEIT_BADID);
    syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
    return TRUE;
}


/*
**  We have an entire article collected; try to post it.  If we're
**  not running, drop the article or just pause and reschedule.
*/
STATIC void
NCpostit(cp)
    register CHANNEL	*cp;
{
    STRING		response;

    /* Note that some use break, some use return here. */
    switch (Mode) {
    default:
	syslog(L_ERROR, "%s internal NCpostit mode %d", CHANname(cp), Mode);
	return;
    case OMpaused:
	SCHANadd(cp, (time_t)(Now.time + PAUSE_RETRY_TIME), (POINTER)&Mode,
	    NCpostit, (POINTER)NULL);
	return;
    case OMrunning:
	response = ARTpost(cp);
	if (atoi(response) == NNTP_TOOKIT_VAL) {
	    cp->Received++;
	    if (cp->Sendid.Size > 3) { /* We be streaming */
		char buff[4];
		(void)sprintf(buff, "%d", NNTP_OK_RECID_VAL);
		cp->Sendid.Data[0] = buff[0];
		cp->Sendid.Data[1] = buff[1];
		cp->Sendid.Data[2] = buff[2];
		response = cp->Sendid.Data;
	    }
	} else {
            cp->Rejected++;
	    if (cp->Sendid.Size) response = cp->Sendid.Data;
        }
	cp->Reported++;
	if (cp->Reported >= NNTP_ACTIVITY_SYNC) {
	    syslog(L_NOTICE,
	    "%s checkpoint seconds %ld accepted %ld refused %ld rejected %ld",
		CHANname(cp), (long)(Now.time - cp->Started),
		cp->Received, cp->Refused, cp->Rejected);
	    cp->Reported = 0;
	}
	NCwritereply(cp, response);
	cp->State = CSgetcmd;
	break;

    case OMthrottled:
	NCwriteshutdown(cp, ModeReason);
	cp->Rejected++;
	break;
    }

    /* Clear the work-in-progress entry. */
    NCclearwip(cp);
}


/*
**  Write-done function.  Close down or set state for what we expect to
**  read next.
*/
STATIC FUNCTYPE
NCwritedone(cp)
    register CHANNEL	*cp;
{
    switch (cp->State) {
    default:
	syslog(L_ERROR, "%s internal NCwritedone state %d",
	    CHANname(cp), cp->State);
	break;

    case CSwritegoodbye:
	if (NCcount > 0)
	    NCcount--;
	CHANclose(cp, CHANname(cp));
	break;

    case CSgetcmd:
    case CSgetauth:
    case CSgetarticle:
    case CSgetrep:
    case CSgetxbatch:
	RCHANadd(cp);
	break;
    }
}



#if	0
/*
**  The "article" command.
*/
STATIC FUNCTYPE
NCarticle(cp)
    register CHANNEL	*cp;
{
    register char	*p;
    register char	*q;
    char		*art;

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("article"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    /* Get the article filenames, and the article header+body. */
    if ((art = ARTreadarticle(HISfilesfor(p))) == NULL) {
	NCwritetext(cp, NNTP_DONTHAVEIT);
	return;
    }

    /* Write it. */
    NCwritetext(cp, NNTP_ARTICLE_FOLLOWS);
    for (p = art; ((q = strchr(p, '\n')) != NULL); p = q + 1) {
	if (*p == '.')
	    WCHANappend(cp, ".", 1);
	WCHANappend(cp, p, q - p);
	WCHANappend(cp, NCterm, STRLEN(NCterm));
    }

    /* Write the terminator. */
    WCHANappend(cp, NCdotterm, STRLEN(NCdotterm));
}
#endif	/* 0 */


/*
**  The "head" command.
*/
STATIC FUNCTYPE
NChead(cp)
    CHANNEL		*cp;
{
    register char	*p;
    register char	*q;
    char		*head;

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("head"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    /* Get the article filenames, and the header. */
    if ((head = ARTreadheader(HISfilesfor(HashMessageID(p)))) == NULL) {
	NCwritetext(cp, NNTP_DONTHAVEIT);
	return;
    }

    /* Write it. */
    NCwritetext(cp, NNTP_HEAD_FOLLOWS);
    for (p = head; ((q = strchr(p, '\n')) != NULL); p = q + 1) {
	if (*p == '.')
	    WCHANappend(cp, ".", 1);
	WCHANappend(cp, p, q - p);
	WCHANappend(cp, NCterm, STRLEN(NCterm));
    }

    /* Write the terminator. */
    WCHANappend(cp, NCdotterm, STRLEN(NCdotterm));
}


/*
**  The "stat" command.
*/
STATIC FUNCTYPE
NCstat(cp)
    CHANNEL		*cp;
{
    register char	*p;
    char		buff[SMBUF];

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("stat"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    /* Get the article filenames; read the header (to make sure not
     * the article is still here). */
    if (ARTreadheader(HISfilesfor(HashMessageID(p))) == NULL) {
	NCwritetext(cp, NNTP_DONTHAVEIT);
	return;
    }

    /* Write the message. */
    (void)sprintf(buff, "%d 0 %s", NNTP_NOTHING_FOLLOWS_VAL, p);
    NCwritetext(cp, buff);
}


/*
**  The "authinfo" command.  Actually, we come in here whenever the
**  channel is in CSgetauth state and we just got a command.
*/
STATIC FUNCTYPE
NCauthinfo(cp)
    register CHANNEL	*cp;
{
    static char		AUTHINFO[] = "authinfo ";
    static char		PASS[] = "pass ";
    static char		USER[] = "user ";
    register char	*p;

    p = cp->In.Data;

    /* Allow the poor sucker to quit. */
    if (caseEQ(p, "quit")) {
	NCquit(cp);
	return;
    }

    /* Otherwise, make sure we're only getting "authinfo" commands. */
    if (!caseEQn(p, AUTHINFO, STRLEN(AUTHINFO))) {
	NCwritetext(cp, NNTP_AUTH_NEEDED);
	return;
    }
    for (p += STRLEN(AUTHINFO); ISWHITE(*p); p++)
	continue;

    /* Ignore "authinfo user" commands, since we only care about the
     * password. */
    if (caseEQn(p, USER, STRLEN(USER))) {
	NCwritetext(cp, NNTP_AUTH_NEXT);
	return;
    }

    /* Now make sure we're getting only "authinfo pass" commands. */
    if (!caseEQn(p, PASS, STRLEN(PASS))) {
	NCwritetext(cp, NNTP_AUTH_NEEDED);
	return;
    }
    for (p += STRLEN(PASS); ISWHITE(*p); p++)
	continue;

    /* Got the password -- is it okay? */
    if (!RCauthorized(cp, p)) {
	NCwritetext(cp, NNTP_AUTH_BAD);
	cp->State = CSwritegoodbye;
    }
    else {
	NCwritetext(cp, NNTP_AUTH_OK);
	cp->State = CSgetcmd;
    }
}

/*
**  The "help" command.
*/
STATIC FUNCTYPE
NChelp(cp)
    register CHANNEL	*cp;
{
    static char		LINE1[] = "For more information, contact \"";
    static char		LINE2[] = "\" at this machine.";
    register NCDISPATCH	*dp;

    NCwritetext(cp, NNTP_HELP_FOLLOWS);
    for (dp = NCcommands; dp < ENDOF(NCcommands); dp++)
	if (dp->Function != NC_unimp) {
            if (!StreamingOff || cp->Streaming ||
                (dp->Function != NCcheck && dp->Function != NCtakethis)) {
                WCHANappend(cp, "\t", 1);
                WCHANappend(cp, dp->Name, dp->Size);
                WCHANappend(cp, NCterm, STRLEN(NCterm));
            }
	}
    WCHANappend(cp, LINE1, STRLEN(LINE1));
    WCHANappend(cp, NEWSMASTER, STRLEN(NEWSMASTER));
    WCHANappend(cp, LINE2, STRLEN(LINE2));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANappend(cp, NCdotterm, STRLEN(NCdotterm));
}

/*
**  The "ihave" command.  Check the Message-ID, and see if we want the
**  article or not.  Set the state appropriately.
*/
STATIC FUNCTYPE
NCihave(cp)
    CHANNEL		*cp;
{
    register char	*p;

    if (AmSlave && !XrefSlave) {
	NCwritetext(cp, NCbadcommand);
	return;
    }

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("ihave"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    if (HIShavearticle(HashMessageID(p))) {
	cp->Refused++;
	NCwritetext(cp, NNTP_HAVEIT);
    }
    else if (WIPinprogress(p, cp, TRUE)) {
	NCwritetext(cp, NNTP_RESENDIT_LATER);
    }
    else {
	NCwritetext(cp, NNTP_SENDIT);
	cp->State = CSgetarticle;
    }
}

/* 
** The "xbatch" command. Set the state appropriately.
*/

STATIC FUNCTYPE
NCxbatch(cp)
    CHANNEL		*cp;
{
    register char	*p;

    if (AmSlave) {
	NCwritetext(cp, NCbadcommand);
	return;
    }

    /* Snip off the batch size */
    for (p = cp->In.Data + STRLEN("xbatch"); ISWHITE(*p); p++)
	continue;

    if (cp->XBatchSize) {
        syslog(L_FATAL, "NCxbatch(): oops, cp->XBatchSize already set to %ld",
	       cp->XBatchSize);
    }

    cp->XBatchSize = atoi(p);
    if (Tracing || cp->Tracing)
        syslog(L_TRACE, "%s will read batch of size %ld",
	       CHANname(cp), cp->XBatchSize);

    if (cp->XBatchSize <= 0) {
        syslog(L_NOTICE, "%s got bad xbatch size %ld",
	       CHANname(cp), cp->XBatchSize);
	NCwritetext(cp, NNTP_XBATCH_BADSIZE);
	return;
    }

#if 1
    /* we prefer not to touch the buffer, NCreader() does enough magic
     * with it
     */
#else
    /* We already know how much data will arrive, so we can allocate
     * sufficient buffer space for the batch in advance. We allocate
     * LOW_WATER + 1 more than needed, so that CHANreadtext() will never
     * reallocate it.
     */
    if (cp->In.Size < cp->XBatchSize + LOW_WATER + 1) {
        DISPOSE(cp->In.Data);
	cp->In.Left = cp->In.Size = cp->XBatchSize + LOW_WATER + 1;
	cp->In.Used = 0;
	cp->In.Data = NEW(char, cp->In.Size);
    }
#endif
    NCwritetext(cp, NNTP_CONT_XBATCH);
    cp->State = CSgetxbatch;
}

/*
**  The "list" command.  Send the active file.
*/
STATIC FUNCTYPE
NClist(cp)
    register CHANNEL	*cp;
{
    register char	*p;
    register char	*q;
    char		*trash;
    char		*end;

    for (p = cp->In.Data + STRLEN("list"); ISWHITE(*p); p++)
	continue;
    if (caseEQ(p, "newsgroups")) {
	trash = p = ReadInFile(_PATH_NEWSGROUPS, (struct stat *)NULL);
	end = p + strlen(p);
    }
    else if (caseEQ(p, "active.times")) {
	trash = p = ReadInFile(_PATH_ACTIVETIMES, (struct stat *)NULL);
	end = p + strlen(p);
    }
    else if (*p == '\0' || (caseEQ(p, "active"))) {
	p = ICDreadactive(&end);
	trash = NULL;
    }
    else {
	NCwritetext(cp, NCbadcommand);
	return;
    }

    /* Loop over all lines, sending the text and \r\n. */
    NCwritetext(cp, NNTP_LIST_FOLLOWS);
    for (; p < end && (q = strchr(p, '\n')) != NULL; p = q + 1) {
	WCHANappend(cp, p, q - p);
	WCHANappend(cp, NCterm, STRLEN(NCterm));
    }
    WCHANappend(cp, NCdotterm, STRLEN(NCdotterm));
    if (trash)
	DISPOSE(trash);
}


/*
**  The "mode" command.  Hand off the channel.
*/
STATIC FUNCTYPE
NCmode(cp)
    register CHANNEL	*cp;
{
    register char	*p;
    HANDOFF		h;

    /* Skip the first word, get the argument. */
    for (p = cp->In.Data + STRLEN("mode"); ISWHITE(*p); p++)
	continue;

    if (caseEQ(p, "reader"))
	h = HOnnrpd;
    else if (caseEQ(p, "query"))
	h = HOnnrqd;
    else if (caseEQ(p, "stream") &&
             (!StreamingOff || (StreamingOff && cp->Streaming))) {
	char buff[16];
	(void)sprintf(buff, "%d StreamOK.", NNTP_OK_STREAM_VAL);
	NCwritetext(cp, buff);
	syslog(L_NOTICE, "%s NCmode \"mode stream\" received",
		CHANname(cp));
	return;
    } else {
	NCwritetext(cp, NCbadcommand);
	return;
    }
    RChandoff(cp->fd, h);
    if (NCcount > 0)
	NCcount--;
    CHANclose(cp, CHANname(cp));
}


/*
**  The "quit" command.  Acknowledge, and set the state to closing down.
*/
STATIC FUNCTYPE NCquit(CHANNEL *cp)
{
    NCclearwip(cp);
    NCwritetext(cp, NNTP_GOODBYE_ACK);
    cp->State = CSwritegoodbye;
}


/*
**  The "xpath" command.  Return the paths for an article is.
*/
STATIC FUNCTYPE
NCxpath(cp)
    CHANNEL		*cp;
{
    static BUFFER	Reply;
    register char	*p;
    register int	i;

    /* Nip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("xpath"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    if ((p = HISfilesfor(HashMessageID(p))) == NULL) {
	NCwritetext(cp, NNTP_DONTHAVEIT);
	return;
    }
    i = 3 + 1 + strlen(p);
    if (Reply.Data == NULL) {
	Reply.Size = i;
	Reply.Data = NEW(char, i + 1);
    }
    else if (Reply.Size < i) {
	Reply.Size = i;
	RENEW(Reply.Data, char, i + 1);
    }
    (void)sprintf(Reply.Data, "%d %s", NNTP_NOTHING_FOLLOWS_VAL, p);
    NCwritetext(cp, Reply.Data);
}


/*
**  The "xreplic" command.  Take an article and the places to file it.
*/
STATIC FUNCTYPE
NCxreplic(cp)
    CHANNEL		*cp;
{
    register char	*p;
    register BUFFER	*bp;
    register int	i;

    if (!RCismaster(cp->Address)) {
	NCwritetext(cp, NCbadcommand);
	return;
    }

    /* Stash the filename arguments. */
    for (p = cp->In.Data + STRLEN("xreplic"); ISWHITE(*p); p++)
	continue;
    i = cp->In.Used - (p - cp->In.Data) + 1;
    bp = &cp->Replic;
    if (bp->Data == NULL) {
	bp->Size = i;
	bp->Data = NEW(char, i);
    }
    BUFFset(bp, p, i);
    bp->Used = bp->Left;

    /* Tell master to send it to us. */
    NCwritetext(cp, NNTP_SENDIT);
    cp->State = CSgetrep;
}


/*
**  The catch-all for inimplemented commands.
*/
STATIC FUNCTYPE
NC_unimp(cp)
    CHANNEL		*cp;
{
    register char	*p;
    char		buff[SMBUF];

    /* Nip off the first word. */
    for (p = cp->In.Data; *p && !ISWHITE(*p); p++)
	continue;
    *p = '\0';
    (void)sprintf(buff, "%d \"%s\" not implemented; try \"help\".",
	    NNTP_BAD_COMMAND_VAL, MaxLength(cp->In.Data, cp->In.Data));
    NCwritetext(cp, buff);
}



/*
**  Remove the \r\n and leading dot escape that the NNTP protocol adds.
*/
STATIC void
NCclean(bp)
    register BUFFER	*bp;
{
    register char	*end;
    register char	*p;
    register char	*dest;

    for (p = bp->Data, dest = p, end = p + bp->Used; p < end; ) {
	if (p[0] == '\r' && p[1] == '\n') {
	    p += 2;
	    *dest++ = '\n';
	    if (p[0] == '.' && p[1] == '.') {
		p += 2;
		*dest++ = '.';
	    }
	}
	else
	    *dest++ = *p++;
    }
    *dest = '\0';
    bp->Used = dest - bp->Data;
}


/*
**  Check whatever data is available on the channel.  If we got the
**  full amount (i.e., the command or the whole article) process it.
*/
STATIC FUNCTYPE NCproc(CHANNEL *cp)
{
    char	        *p;
    char                *q;
    NCDISPATCH   	*dp;
    BUFFER	        *bp;
    char		buff[SMBUF];
    char		*av[2];
    int			i;

    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s NCproc Used=%d",
	    CHANname(cp), cp->In.Used);

    bp = &cp->In;
    if (bp->Used == 0)
	return;

    p = &bp->Data[bp->Used];
    for ( ; ; ) {
	cp->Rest = 0;
	cp->SaveUsed = bp->Used;
	if (Tracing || cp->Tracing)
	    if (bp->Used > 15)
		syslog(L_TRACE, "%s NCproc state=%d next \"%.15s\"",
		    CHANname(cp), cp->State, bp->Data);
	switch (cp->State) {
	default:
	    syslog(L_ERROR, "%s internal NCproc state %d",
		CHANname(cp), cp->State);
	    break;

	case CSgetcmd:
	case CSgetauth:
	    /* Did we get the whole command, terminated with "\r\n"? */
	    for (i = 0; (i < bp->Used) && (bp->Data[i] != '\n'); i++) ;
	    if (i < bp->Used) cp->Rest = bp->Used = ++i;
	    else {
		cp->Rest = 0;
		break;	/* come back later for rest of line */
	    }
	    if (cp->Rest < 2) break;
	    p = &bp->Data[cp->Rest];
	    if (p[-2] != '\r' || p[-1] != '\n') { /* probably in an article */
		int j;

		syslog(L_NOTICE, "%s bad_command %s",
		    CHANname(cp), MaxLength(bp->Data, bp->Data));
		NCwritetext(cp, NCbadcommand);
		if (++(cp->BadCommands) >= BAD_COMMAND_COUNT) {
		    cp->State = CSwritegoodbye;
		    cp->Rest = cp->SaveUsed;
		    break;
		}
		for (j = i + 1; j < cp->SaveUsed; j++)
		    if (bp->Data[j] == '\n') {
			if (bp->Data[j - 1] == '\r') break;
			else cp->Rest = bp->Used = j + 1;
		    }
		break;
	    }
	    p[-2] = '\0';
	    bp->Used -= 2;

	    /* Ignore blank lines. */
	    if (bp->Data[0] == '\0')
		break;
	    if (Tracing || cp->Tracing)
		syslog(L_TRACE, "%s < %s", CHANname(cp), bp->Data);

	    /* We got something -- stop sleeping (in case we were). */
	    SCHANremove(cp);
	    if (cp->Argument != NULL) {
		DISPOSE(cp->Argument);
		cp->Argument = NULL;
	    }

	    if (cp->State == CSgetauth) {
		if (caseEQn(bp->Data, "mode", 4))
		    NCmode(cp);
		else
		    NCauthinfo(cp);
		break;
	    }

	    /* Loop through the command table. */
	    for (p = bp->Data, dp = NCcommands; dp < ENDOF(NCcommands); dp++)
		if (caseEQn(p, dp->Name, dp->Size)) {
                    /* ignore the streaming commands if necessary. */
                    if (!StreamingOff || cp->Streaming ||
                        (dp->Function != NCcheck && dp->Function != NCtakethis)) {
                        (*dp->Function)(cp);
                        cp->BadCommands = 0;
                        break;
                    }
		}
            
	    if (dp == ENDOF(NCcommands)) {
		NCwritetext(cp, NCbadcommand);
		if (++(cp->BadCommands) >= BAD_COMMAND_COUNT) {
		    cp->State = CSwritegoodbye;
		    cp->Rest = cp->SaveUsed;
		}
		for (i = 0; (p = NCquietlist[i]) != NULL; i++)
		    if (caseEQ(p, bp->Data))
			break;
		if (p == NULL)
		    syslog(L_NOTICE, "%s bad_command %s",
			CHANname(cp), MaxLength(bp->Data, bp->Data));
	    }
	    break;

	case CSgetarticle:
	case CSgetrep:
	    /* Check for the null article. */
	    if ((bp->Used >= 3) && (bp->Data[0] == '.')
	     && (bp->Data[1] == '\r') && (bp->Data[2] == '\n')) {
		cp->Rest = 3;	/* null article (canceled?) */
		cp->Rejected++;
#if 0
                syslog(L_NOTICE, "%s empty article", CHANname(cp)) ;
#endif
		if (cp->Sendid.Size > 3) { /* We be streaming */
		    char buff[4];
		    (void)sprintf(buff, "%d", NNTP_ERR_FAILID_VAL);
		    cp->Sendid.Data[0] = buff[0];
		    cp->Sendid.Data[1] = buff[1];
		    cp->Sendid.Data[2] = buff[2];
		    NCwritereply(cp, cp->Sendid.Data);
		}
		else NCwritetext(cp, NNTP_REJECTIT_EMPTY);
		cp->State = CSgetcmd;
		bp->Used = 0;

		/* Clear the work-in-progress entry. */
		NCclearwip(cp);
		break;
	    }
	    /* Reading an article; look for "\r\n.\r\n" terminator. */
	    if (cp->Lastch > 5) i = cp->Lastch; /* only look at new data */
	    else		i = 5;
	    for ( ; i <= bp->Used; i++) {
		if ((bp->Data[i - 5] == '\r')
		 && (bp->Data[i - 4] == '\n')
		 && (bp->Data[i - 3] == '.')
		 && (bp->Data[i - 2] == '\r')
		 && (bp->Data[i - 1] == '\n')) {
		    cp->Rest = bp->Used = i;
		    p = &bp->Data[i];
		    break;
		}
	    }
	    cp->Lastch = i;
	    if (i > bp->Used) {	/* did not find terminator */
		/* Check for big articles. */
		if (LargestArticle > SAVE_AMT && bp->Used > LargestArticle) {
		    /* Make some room, saving only the last few bytes. */
		    for (p = bp->Data, i = 0; i < SAVE_AMT; p++, i++)
			p[0] = p[bp->Used - SAVE_AMT];
		    cp->LargeArtSize += bp->Used - SAVE_AMT;
		    bp->Used = cp->Lastch = SAVE_AMT;
		    cp->State = CSeatarticle;
		}
		cp->Rest = 0;
		break;
	    }
	    if (Mode == OMpaused) { /* defer processing while paused */
		cp->Rest = 0;
		bp->Used = cp->SaveUsed;
		SCHANadd(cp, (time_t)(Now.time + PAUSE_RETRY_TIME),
		    (POINTER)&Mode, NCproc, (POINTER)NULL);
		return;
	    }

	    /* Strip article terminator and post the article. */
	    p[-3] = '\0';
	    bp->Used -= 2;
	    SCHANremove(cp);
	    if (cp->Argument != NULL) {
		DISPOSE(cp->Argument);
		cp->Argument = NULL;
	    }
	    if (!WireFormat)
  	        NCclean(bp);
	    NCpostit(cp);
	    cp->State = CSgetcmd;
	    break;

	case CSeatarticle:
	    /* Eat the article and then complain that it was too large */
	    /* Reading an article; look for "\r\n.\r\n" terminator. */
	    if (cp->Lastch > 5) i = cp->Lastch; /* only look at new data */
	    else		i = 5;
	    for ( ; i <= bp->Used; i++) {
		if ((bp->Data[i - 5] == '\r')
		 && (bp->Data[i - 4] == '\n')
		 && (bp->Data[i - 3] == '.')
		 && (bp->Data[i - 2] == '\r')
		 && (bp->Data[i - 1] == '\n')) {
		    cp->Rest = bp->Used = i;
		    p = &bp->Data[i];
		    break;
		}
	    }
	    if (i <= bp->Used) {	/* did find terminator */
		/* Reached the end of the article. */
		SCHANremove(cp);
		if (cp->Argument != NULL) {
		    DISPOSE(cp->Argument);
		    cp->Argument = NULL;
		}
		i = cp->LargeArtSize + bp->Used;
		syslog(L_ERROR, "%s internal rejecting huge article (%d > %d)",
		    CHANname(cp), i, LargestArticle);
		cp->LargeArtSize = 0;
		(void)sprintf(buff, "%d Article exceeds local limit of %ld bytes",
			NNTP_REJECTIT_VAL, LargestArticle);
		if (cp->Sendid.Size) NCwritetext(cp, cp->Sendid.Data);
		else NCwritetext(cp, buff);
		cp->State = CSgetcmd;
		cp->Rejected++;

		/* Write a local cancel entry so nobody else gives it to us. */
		    if (!HISremember(cp->CurrentMessageIDHash))
			syslog(L_ERROR, "%s cant cancel %s %s", LogName, av[0], q); 

		/* Clear the work-in-progress entry. */
		NCclearwip(cp);

		/*
		 * only free and allocate the buffer back to
		 * START_BUFF_SIZE if there's nothing in the buffer we
		 * need to save (i.e., following commands.
		 * if there is, then we're probably in streaming mode,
		 * so probably not much point in trying to keep the
		 * buffers minimal anyway...
		 */
		if (bp->Used == cp->SaveUsed) {
		    /* Reset input buffer to the default size; don't let realloc
		     * be lazy. */
		    DISPOSE(bp->Data);
		    bp->Size = START_BUFF_SIZE;
		    bp->Used = 0;
		    bp->Data = NEW(char, bp->Size);
		    cp->SaveUsed = cp->Rest = cp->Lastch = 0;
		}
	    }
	    else if (bp->Used > 8 * 1024) {
		/* Make some room; save the last few bytes of the article */
		for (p = bp->Data, i = 0; i < SAVE_AMT; p++, i++)
		    p[0] = p[bp->Used - SAVE_AMT + 0];
		cp->LargeArtSize += bp->Used - SAVE_AMT;
		bp->Used = cp->Lastch = SAVE_AMT;
		cp->Rest = 0;
	    }
	    break;
	case CSgetxbatch:
	    /* if the batch is complete, write it out into the in.coming
	    * directory with an unique timestamp, and start rnews on it.
	    */
	    if (Tracing || cp->Tracing)
		syslog(L_TRACE, "%s CSgetxbatch: now %ld of %ld bytes",
			CHANname(cp), bp->Used, cp->XBatchSize);

	    if (bp->Used < cp->XBatchSize) {
		cp->Rest = 0;
		break;	/* give us more data */
	    }
	    cp->Rest = cp->XBatchSize;

	    /* now do something with the batch */
	    {
		char buff[SMBUF], buff2[SMBUF];
		int fd, oerrno, failed;
		long now;

		now = time(NULL);
		failed = 0;
		/* time+channel file descriptor should make an unique file name */
		sprintf(buff, "%s/%ld%d.tmp", _PATH_XBATCHES, now, cp->fd);
		fd = open(buff, O_WRONLY|O_CREAT|O_EXCL, ARTFILE_MODE);
		if (fd < 0) {
		    oerrno = errno;
		    failed = 1;
		    syslog(L_ERROR, "%s cannot open outfile %s for xbatch: %m",
			    CHANname(cp), buff);
		    sprintf(buff, "%s cant create file: %s",
			    NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
		    NCwritetext(cp, buff);
		} else {
		    if (write(fd, cp->In.Data, cp->XBatchSize) != cp->XBatchSize) {
			oerrno = errno;
			syslog(L_ERROR, "%s cant write batch to file %s: %m",
				CHANname(cp), buff);
			sprintf(buff, "%s cant write batch to file: %s",
				NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
			NCwritetext(cp, buff);
			failed = 1;
		    }
		}
		if (fd >= 0 && close(fd) != 0) {
		    oerrno = errno;
		    syslog(L_ERROR, "%s error closing batch file %s: %m",
			    CHANname(cp), failed ? "" : buff);
		    sprintf(buff, "%s error closing batch file: %s",
			    NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
		    NCwritetext(cp, buff);
		    failed = 1;
		}
		sprintf(buff2, "%s/%ld%d.x", _PATH_XBATCHES, now, cp->fd);
		if (rename(buff, buff2)) {
		    oerrno = errno;
		    syslog(L_ERROR, "%s cant rename %s to %s: %m",
			    CHANname(cp), failed ? "" : buff, buff2);
		    sprintf(buff, "%s cant rename batch to %s: %s",
			    NNTP_RESENDIT_XBATCHERR, buff2, strerror(oerrno));
		    NCwritetext(cp, buff);
		    failed = 1;
		}
		cp->Reported++;
		if (!failed) {
		    NCwritetext(cp, NNTP_OK_XBATCHED);
		    cp->Received++;
		} else
		    cp->Rejected++;
	    }
	    syslog(L_NOTICE, "%s accepted batch size %ld",
		   CHANname(cp), cp->XBatchSize);
	    cp->State = CSgetcmd;
	    
	    /* Clear the work-in-progress entry. */
	    NCclearwip(cp);

#if 1
	    /* leave the input buffer as it is, there is a fair chance
		* that we will get another xbatch on this channel.
		* The buffer will finally be disposed at connection shutdown.
		*/
#else
	    /* Reset input buffer to the default size; don't let realloc 
		* be lazy. */
	    DISPOSE(bp->Data);
	    bp->Size = START_BUFF_SIZE;
	    bp->Used = 0;
	    bp->Data = NEW(char, bp->Size);
#endif
	    break;
	}
	if (Tracing || cp->Tracing)
		syslog(L_TRACE, "%s NCproc Rest=%d Used=%d SaveUsed=%d",
		    CHANname(cp), cp->Rest, bp->Used, cp->SaveUsed);

	if (cp->Rest > 0) {
	    if (cp->Rest < cp->SaveUsed) { /* more commands in buffer */
		bp->Used = cp->SaveUsed = cp->SaveUsed - cp->Rest;
		/* It would be nice to avoid this copy but that
		** would require changes to the bp structure and
		** the way it is used.
		*/
		(void)memmove((POINTER)bp->Data, (POINTER)&bp->Data[cp->Rest], (SIZE_T)bp->Used);
		cp->Rest = cp->Lastch = 0;
	    } else {
		bp->Used = cp->Lastch = 0;
		break;
	    }
	} else break;
    }
}


/*
**  Read whatever data is available on the channel.  If we got the
**  full amount (i.e., the command or the whole article) process it.
*/
STATIC FUNCTYPE
NCreader(cp)
    register CHANNEL	*cp;
{
    int			i;

    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s NCreader Used=%d",
	    CHANname(cp), cp->In.Used);

    /* Read any data that's there; ignore errors (retry next time it's our
     * turn) and if we got nothing, then it's EOF so mark it closed. */
    if ((i = CHANreadtext(cp)) <= 0) {
#ifdef POLL_BUG
        /* return of -2 indicates EAGAIN, for SUNOS5.4 poll() bug workaround */
        if (i == -2) {
            return;
        }
#endif
	if (i == 0 || cp->BadReads++ >= BAD_IO_COUNT) {
	    if (NCcount > 0)
		NCcount--;
	    CHANclose(cp, CHANname(cp));
	}
	return;
    }

    NCproc(cp);	    /* check and process data */
}



/*
**  Set up the NNTP channel state.
*/
void
NCsetup(i)
    register int	i;
{
    register NCDISPATCH	*dp;
    char		*p;
    char		buff[SMBUF];

    /* Set the greeting message. */
    if ((p = GetConfigValue(_CONF_PATHHOST)) == NULL)
	/* Worked in main, now it fails?  Curious. */
	p = Path.Data;
    (void)sprintf(buff, "%d %s InterNetNews server %s ready",
	    NNTP_POSTOK_VAL, p, Version);
    NCgreeting = COPY(buff);

    /* Get the length of every command. */
    for (dp = NCcommands; dp < ENDOF(NCcommands); dp++)
	dp->Size = strlen(dp->Name);
}


/*
**  Tear down our state.
*/
void
NCclose()
{
    register CHANNEL	*cp;
    int			j;

    /* Close all incoming channels. */
    for (j = 0; (cp = CHANiter(&j, CTnntp)) != NULL; ) {
	if (NCcount > 0)
	    NCcount--;
	CHANclose(cp, CHANname(cp));
    }
}


/*
**  Create an NNTP channel and print the greeting message.
*/
CHANNEL *
NCcreate(fd, MustAuthorize, IsLocal)
    int			fd;
    BOOL		MustAuthorize;
    BOOL		IsLocal;
{
    register CHANNEL	*cp;
    int			i;

    /* Create the channel. */
    cp = CHANcreate(fd, CTnntp, MustAuthorize ? CSgetauth : CSgetcmd,
	    NCreader, NCwritedone);

    NCclearwip(cp);
#if defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF) 
#if defined (DO_SET_SOCKOPT)
    i = 24 * 1024;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&i, sizeof i) < 0)
	syslog(L_ERROR, "%s cant setsockopt(SNDBUF) %m", CHANname(cp));
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof i) < 0)
	syslog(L_ERROR, "%s cant setsockopt(RCVBUF) %m", CHANname(cp));
#endif
#endif	/* defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF) */

#if	defined(SOL_SOCKET) && defined(SO_KEEPALIVE)
#if defined (DO_SET_SOCKOPT)
    /* Set KEEPALIVE to catch broken socket connections. */
    i = 1;
    if (setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE, (char *)&i, sizeof i) < 0)
        syslog(L_ERROR, "%s cant setsockopt(KEEPALIVE) %m", CHANname(cp));
#endif
#endif /* defined(SOL_SOCKET) && defined(SO_KEEPALIVE) */

    /* Now check our operating mode. */
    NCcount++;
    if (Mode == OMthrottled) {
	NCwriteshutdown(cp, ModeReason);
	return cp;
    }
    if (RejectReason) {
	NCwriteshutdown(cp, RejectReason);
	return cp;
    }

    /* See if we have too many channels. */
    if (!IsLocal && MaxIncoming && NCcount >= MaxIncoming && !RCnolimit(cp)) {
	/* Recount, just in case we got out of sync. */
	for (NCcount = 0, i = 0; CHANiter(&i, CTnntp) != NULL; )
	    NCcount++;
	if (NCcount >= MaxIncoming) {
	    NCwriteshutdown(cp, "Too many connections");
	    return cp;
	}
    }
    cp->BadReads = 0;
    cp->BadCommands = 0;
    NCwritetext(cp, NCgreeting);
    return cp;
}



/* These modules support the streaming option to tranfer articles
** faster.
*/

/*
**  The "check" command.  Check the Message-ID, and see if we want the
**  article or not.  Stay in command state.
*/
STATIC FUNCTYPE
NCcheck(cp)
    CHANNEL		*cp;
{
    register char	*p;
    int			msglen;

    if (AmSlave && !XrefSlave) {
	NCwritetext(cp, NCbadcommand);
	return;
    }

    /* Snip off the Message-ID. */
    for (p = cp->In.Data; !ISWHITE(*p); p++)
	continue;
    for ( ; ISWHITE(*p); p++)
	continue;
    msglen = strlen(p) + 5; /* 3 digits + space + id + null */
    if (cp->Sendid.Size < msglen) {
	if (cp->Sendid.Size > 0) DISPOSE(cp->Sendid.Data);
	if (msglen > MAXHEADERSIZE) cp->Sendid.Size = msglen;
	else cp->Sendid.Size = MAXHEADERSIZE;
	cp->Sendid.Data = NEW(char, cp->Sendid.Size);
    }
    if (!ARTidok(p)) {
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
	return;
    }

    if (HIShavearticle(HashMessageID(p))) {
	cp->Refused++;
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
    } else if (WIPinprogress(p, cp, FALSE)) {
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_RESENDID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
    } else {
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_OK_SENDID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
    }
    /* stay in command mode */
}

/*
**  The "takethis" command.  Article follows.
**  Remember <id> for later ack.
*/
STATIC FUNCTYPE NCtakethis(CHANNEL *cp)
{
    char	        *p;
    int			msglen;
    WIP                 *wp;

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("takethis"); ISWHITE(*p); p++)
	continue;
    for ( ; ISWHITE(*p); p++)
	continue;
    if (!ARTidok(p)) {
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
    }
    msglen = strlen(p) + 5; /* 3 digits + space + id + null */
    if (cp->Sendid.Size < msglen) {
	if (cp->Sendid.Size > 0) DISPOSE(cp->Sendid.Data);
	if (msglen > MAXHEADERSIZE) cp->Sendid.Size = msglen;
	else cp->Sendid.Size = MAXHEADERSIZE;
	cp->Sendid.Data = NEW(char, cp->Sendid.Size);
    }
    /* save ID for later NACK or ACK */
    (void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_FAILID_VAL, p);

    cp->State = CSgetarticle;
    /* set WIP for benefit of later code in NCreader */
    wp = WIPnew(p, cp);
    cp->CurrentMessageIDHash = wp->MessageID;
}
