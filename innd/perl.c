/*  $Id$
**
**  Perl filtering support for innd.
**
**  Originally written by Christophe Wolfhugel <wolf@pasteur.fr> (although
**  he wouldn't recognise it anymore so don't blame him) and modified,
**  expanded and tweaked since by James Brister, Jeremy Nixon, Ed Mooring,
**  Russell Vincent, and Russ Allbery.
**
**  This file should contain all innd-specific Perl linkage.  Linkage
**  applicable to both innd and nnrpd should go into lib/perl.c instead.
**
**  We are assuming Perl 5.004 or later.
**
**  Future work:
**
**   - What we're doing with Path headers right now doesn't work for folded
**     headers.  It's also kind of gross.  There has to be a better way of
**     handling this.
**
**   - The breakdown between this file, lib/perl.c, and nnrpd/perl.c should
**     be rethought, ideally in the light of supporting multiple filters in
**     different languages.
**
**   - We're still calling strlen() on artBody, which should be avoidable
**     since we've already walked it several times.  We should just cache
**     the length somewhere for speed.
**
**   - Variable and key names should be standardized between this and nnrpd.
**
**   - The XS code is still calling CC* functions.  The common code between
**     the two control interfaces should be factored out into the rest of
**     innd instead.
**
**   - There's a needless perl_get_cv() call for *every message ID* offered
**     to the server right now.  We need to stash whether that filter is
**     active.
*/

#include "config.h"

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if defined(DO_PERL)

#include "clibrary.h"
#include "innd.h"
#include "art.h"

/* Linux doesn't have bool, yet sets _G_HAVE_BOOL to true.  Hello? */
#ifdef DO_NEED_BOOL
typedef int bool;
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "ppport.h"

/* From lib/perl.c. */
extern BOOL		PerlFilterActive;

/* From art.c.  Ew.  Need header parsing that doesn't use globals. */
extern ARTHEADER	ARTheaders[], *ARTheadersENDOF;
extern char		*filterPath;

/*
**  Run an incoming article through the Perl article filter.  Returns NULL
**  accept the article or a rejection message to reject it.
*/
char *
PLartfilter(char *artBody, int lines)
{
    dSP;
    ARTHEADER * hp;
    HV *        hdr;
    CV *        filter;
    int         rc;
    char *      p;
    char        save = '\0';
    static SV * body = NULL;
    static char buf[256];

    if (!PerlFilterActive) return NULL;
    filter = perl_get_cv("filter_art", 0);
    if (!filter) return NULL;

    /* Create %hdr and stash a copy of every known header.  Path has to be
       handled separately since it's been munged by article processing. */
    hdr = perl_get_hv("hdr", 1);
    for (hp = ARTheaders; hp < ARTheadersENDOF; hp++) {
        if (hp->Found && hp->Value && strcmp(hp->Name, "Path") != 0)
            hv_store(hdr, (char *) hp->Name, strlen(hp->Name),
                     newSVpv(hp->Value, 0), 0);
    }
    if (filterPath) {
        p = strpbrk(filterPath, "\r\n");
        if (p) {
	    save = *p;
	    *p = '\0';
	}
        hv_store(hdr, "Path", 4, newSVpv(filterPath, 0), 0);
        if (p) *p = save;
    }

    /* Store the article body.  We don't want to make another copy of it,
       since it could potentially be quite large.  Instead, stash the
       pointer in the static SV * body.  We set LEN to 0 and inc the
       refcount to tell Perl not to free it (either one should be enough).
       Requires 5.004.  In testing, this produced a 17% speed improvement
       over making a copy of the article body for a fairly heavy filter. */
    if (artBody) {
        if (!body) {
            body = newSV(0);
            (void) SvUPGRADE(body, SVt_PV);
        }
        SvPVX(body) = artBody;
        SvCUR_set(body, strlen(artBody));
        SvLEN_set(body, 0);
        SvPOK_on(body);
        SvREADONLY_on(body);
        SvREFCNT_inc(body);
        hv_store(hdr, "__BODY__", 8, body, 0);
    }

    hv_store(hdr, "__LINES__", 9, newSViv(lines), 0);

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    rc = perl_call_sv((SV *) filter, G_EVAL|G_SCALAR|G_NOARGS);
    SPAGAIN;

    hv_undef(hdr);

    /* Check $@, which will be set if the sub died. */
    buf[0] = '\0';
    if (SvTRUE(ERRSV)) {
        syslog(L_ERROR, "Perl function filter_art died: %s",
               SvPV(ERRSV, PL_na));
        (void) POPs;
        PerlFilter(FALSE);
    } else if (rc == 1) {
        p = POPp;
        if (p && *p) {
            strncpy(buf, p, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
        }
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
    return (buf[0] != '\0') ? buf : NULL;
}


/*
**  Run an incoming message ID from CHECK or IHAVE through the Perl filter.
**  Returns NULL to accept the article or a rejection message to reject it.
*/
char *
PLmidfilter(char *messageID)
{
    dSP;
    CV          *filter;
    int         rc;
    char        *p;
    static char buf[256];

    if (!PerlFilterActive) return NULL;
    filter = perl_get_cv("filter_messageid", 0);
    if (!filter) return NULL;

    /* Pass filter_messageid() the message ID on the Perl stack. */
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVpv(messageID, 0)));
    PUTBACK;
    rc = perl_call_sv((SV *) filter, G_EVAL|G_SCALAR);
    SPAGAIN;

    /* Check $@, which will be set if the sub died. */
    buf[0] = '\0';
    if (SvTRUE(ERRSV)) {
        syslog(L_ERROR, "Perl function filter_messageid died: %s",
               SvPV(ERRSV, PL_na));
        (void) POPs;
        PerlFilter(FALSE);
    } else if (rc == 1) {
        p = POPp;
        if (p && *p) {
            strncpy(buf, p, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
        }
    }
    
    PUTBACK;
    FREETMPS;
    LEAVE;
    return (buf[0] != '\0') ? buf : NULL;
}


/*
**  Call a Perl sub on any change in INN's mode, passing in the old and new
**  mode and the reason.
*/
void
PLmode(OPERATINGMODE Mode, OPERATINGMODE NewMode, char *reason)
{
    dSP;
    HV          *mode;
    CV          *filter;

    filter = perl_get_cv("filter_mode", 0);
    if (!filter) return;

    /* Current mode goes into $mode{Mode}, new mode in $mode{NewMode}, and
       the reason in $mode{reason}. */
    mode = perl_get_hv("mode", 1);

    if (Mode == OMrunning)
        hv_store(mode, "Mode", 4, newSVpv("running", 0), 0);
    if (Mode == OMpaused)
        hv_store(mode, "Mode", 4, newSVpv("paused", 0), 0);
    if (Mode == OMthrottled)
        hv_store(mode, "Mode", 4, newSVpv("throttled", 0), 0);

    if (NewMode == OMrunning)
        hv_store(mode, "NewMode", 7, newSVpv("running", 0), 0);
    if (NewMode == OMpaused)
        hv_store(mode, "NewMode", 7, newSVpv("paused", 0), 0);
    if (NewMode == OMthrottled)
        hv_store(mode, "NewMode", 7, newSVpv("throttled", 0), 0);

    hv_store(mode, "reason", 6, newSVpv(reason, 0), 0);

    PUSHMARK(SP);
    perl_call_sv((SV *) filter, G_EVAL|G_DISCARD|G_NOARGS);

    /* Check $@, which will be set if the sub died. */
    if (SvTRUE(ERRSV)) {
        syslog(L_ERROR, "Perl function filter_mode died: %s",
                SvPV(ERRSV, PL_na));
        (void) POPs;
        PerlFilter(FALSE);
    }
}


/*
**  The remainder of this file are XS callbacks visible to embedded Perl
**  code to perform various innd functions.  They were originally written by
**  Ed Mooring (mooring@acm.org) on May 14, 1998, and have since been split
**  between this file and lib/perl.c (which has the ones that can also be
**  used in nnrpd).  The function that registers them at startup is at the
**  end.
*/

/*
**  Add an entry to history.  Takes message ID and optionally arrival,
**  article, and expire times and storage API token.  If the times aren't
**  given, they default to now.  If the token isn't given, that field will
**  be left empty.  Returns boolean success.
*/
XS(XS_INN_addhist)
{
    dXSARGS;
    int         i;
    char        tbuff[32];
    char*       parambuf[6];

    if (items < 1 || items > 5)
        croak("Usage INN::addhist(msgid,[arrival,articletime,expire,token])");

    for (i = 0; i < items; i++)
        parambuf[i] = (char *) SvPV(ST(0), PL_na);

    /* If any of the times are missing, they should default to now. */
    if (i < 4) {
        sprintf(tbuff, "%ld", time((long *) 0));
        for (; i < 4; i++)
            parambuf[i] = tbuff;
    }

    /* The token defaults to an empty string. */
    if (i == 4)
        parambuf[4] = "";

    parambuf[5] = NULL;

    /* CCaddhist returns NULL on success. */
    if (CCaddhist(parambuf))
        XSRETURN_NO;
    else
        XSRETURN_YES;
}


/*
**  Takes the message ID of an article and returns the full article as a
**  string or undef if the article wasn't found.  It will be converted from
**  wire format to native format.  Note that this call isn't particularly
**  optimized or cheap.
*/
XS(XS_INN_article)
{
    dXSARGS;
    char *      msgid;
    TOKEN *     token;
    ARTHANDLE * art;
    char *      p;
    int         len;

    if (items != 1)
	croak("Usage: INN::article(msgid)");

    /* Get the article token from the message ID and the history file. */
    msgid = (char *) SvPV(ST(0), PL_na);
    token = HISfilesfor(HashMessageID(msgid));
    if (token == NULL) XSRETURN_UNDEF;

    /* Retrieve the article and convert it from wire format. */
    art = SMretrieve(*token, RETR_ALL);
    if (art == NULL) XSRETURN_UNDEF;
    p = FromWireFmt(art->data, art->len, &len);
    SMfreearticle(art);

    /* Push a copy of the article onto the Perl stack, free our temporary
       memory allocation, and return the article to Perl. */
    ST(0) = sv_2mortal(newSVpv(p, len));
    DISPOSE(p);
    XSRETURN(1);
}


/*
**  Cancel a message by message ID; returns boolean success.  Equivalent to
**  ctlinnd cancel <message>.
*/
XS(XS_INN_cancel)
{
    dXSARGS;
    char        *msgid;
    char        *parambuf[2];

    if (items != 1)
        croak("Usage: INN::cancel(msgid)");

    msgid = (char *) SvPV(ST(0), PL_na);
    parambuf[0] = msgid;
    parambuf[1] = NULL;

    /* CCcancel returns NULL on success. */
    if (CCcancel(parambuf))
        XSRETURN_NO;
    else
        XSRETURN_YES;
}


/*
**  Return the files for a given message ID, taken from the history file.
**  This function should really be named INN::token() and probably will be
**  some day.
*/
XS(XS_INN_filesfor)
{
    dXSARGS;
    char        *msgid;
    TOKEN       *token;

    if (items != 1)
        croak("Usage: INN::filesfor(msgid)");

    msgid = (char *) SvPV(ST(0), PL_na);
    token = HISfilesfor(HashMessageID(msgid));
    if (token) {
        XSRETURN_PV(TokenToText(*token));
    } else {
        XSRETURN_UNDEF;
    }
}


/*
**  Whether message ID is in the history file; returns boolean.
*/
XS(XS_INN_havehist)
{
    dXSARGS;
    char        *msgid;

    if (items != 1)
        croak("Usage: INN::havehist(msgid)");

    msgid = (char *) SvPV(ST(0), PL_na);
    if (HIShavearticle(HashMessageID(msgid)))
        XSRETURN_YES;
    else
        XSRETURN_NO;
}


/*
**  Takes the message ID of an article and returns the article headers as
**  a string or undef if the article wasn't found.  Each line of the header
**  will end with \n.
*/
XS(XS_INN_head)
{
    dXSARGS;
    char *      msgid;
    TOKEN *     token;
    ARTHANDLE * art;
    char *      p;
    int         len;

    if (items != 1)
        croak("Usage: INN::head(msgid)");

    /* Get the article token from the message ID and the history file. */
    msgid = (char *) SvPV(ST(0), PL_na);
    token = HISfilesfor(HashMessageID(msgid));
    if (token == NULL) XSRETURN_UNDEF;

    /* Retrieve the article header and convert it from wire format. */
    art = SMretrieve(*token, RETR_HEAD);
    if (art == NULL) XSRETURN_UNDEF;
    p = FromWireFmt(art->data, art->len, &len);
    SMfreearticle(art);

    /* Push a copy of the article header onto the Perl stack, free our
       temporary memory allocation, and return the header to Perl. */
    ST(0) = sv_2mortal(newSVpv(p, len));
    DISPOSE(p);
    XSRETURN(1);
}


/*
**  Returns the active file flag for a newsgroup or undef if it isn't in the
**  active file.
*/
XS(XS_INN_newsgroup)
{
    dXSARGS;
    char *      newsgroup;
    NEWSGROUP * ngp;
    char *      end;
    int         size;

    if (items != 1)
        croak("Usage: INN::newsgroup(group)");
    newsgroup = (char *) SvPV(ST(0), PL_na);

    ngp = NGfind(newsgroup);
    if (!ngp) {
        XSRETURN_UNDEF;
    } else {
        /* ngp->Rest is newline-terminated; find the end. */
        end = strchr(ngp->Rest, '\n');
        if (end == NULL) {
            size = strlen(ngp->Rest);
        } else {
            size = end - ngp->Rest;
        }
        ST(0) = sv_2mortal(newSVpv(ngp->Rest, size));
        XSRETURN(1);
    }
}


/*
**  Initialize the XS callbacks defined in this file.
*/
void
PLxsinit()
{
    newXS("INN::addhist", XS_INN_addhist, "perl.c");
    newXS("INN::article", XS_INN_article, "perl.c");
    newXS("INN::cancel", XS_INN_cancel, "perl.c");
    newXS("INN::havehist", XS_INN_havehist, "perl.c");
    newXS("INN::head", XS_INN_head, "perl.c");
    newXS("INN::newsgroup", XS_INN_newsgroup, "perl.c");
    newXS("INN::filesfor", XS_INN_filesfor, "perl.c");
}

#endif /* defined(DO_PERL) */
