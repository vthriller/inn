/*  $Id$
**
**  Here be a set of NNTP response codes as defined in RFC977 and elsewhere.
**  The reponse codes are three digits, RFI, defined like this:
**	R, Response:
**		1xx	Informative message
**		2xx	Command ok
**		3xx	Command ok so far, send the rest of it.
**		4xx	Command was correct, but couldn't be performed for
**			some reason.
**		5xx	Command unimplemented, or incorrect, or a serious
**			program error occurred.
**	F, Function:
**		x0x	Connection, setup, and miscellaneous messages
**		x1x	Newsgroup selection
**		x2x	Article selection
**		x3x	Distribution functions
**		x4x	Posting
**		x8x	Nonstandard extensions (AUTHINFO, XGTITLE)
**		x9x	Debugging output
**	I, Information:
**		No defined semantics
*/

#include "inn/nntp.h"

#define NNTP_BAD_COMMAND		"500 Syntax error or bad command"
#define NNTP_ACCESS			"502 Permission denied"
#define NNTP_GOODBYE_ACK		"205 ."
#define NNTP_GOODBYE			"400"
#define NNTP_HAVEIT			"435 Duplicate"
#define NNTP_HAVEIT_BADID		"435 Bad Message-ID"
#define NNTP_LIST_FOLLOWS		"215"
#define NNTP_HELP_FOLLOWS		"100 Legal commands"
#define NNTP_ARTICLE_FOLLOWS		"220"
#define NNTP_HEAD_FOLLOWS		"221"
#define NNTP_POSTOK			"200"
#define NNTP_REJECTIT_EMPTY		"437 Empty article"
#define NNTP_DONTHAVEIT			"430"
#define NNTP_RESENDIT_LATER             "436 Retry later"
#define NNTP_POSTEDOK			"240 Article posted"
#define NNTP_SENDIT			"335"
#define NNTP_SYNTAX_USE			"501 Bad command use"
#define NNTP_BAD_SUBCMD			"501 Bad subcommand"
#define NNTP_TOOKIT			"235"
#define NNTP_NOTINGROUP			"412 Not in a newsgroup"
#define NNTP_NOSUCHGROUP		"411 No such group"
#define NNTP_NEWNEWSOK			"230 New news follows"
#define NNTP_NOARTINGRP			"423 Bad article number"
#define NNTP_NOCURRART			"420 No current article"
#define NNTP_CANTPOST			"440 Posting not allowed"

/* new entries for the "streaming" protocol */
/* response to "mode stream" else 500 if stream not supported */

/* response to "check <id>".  Must include ID of article.
** Example: "431 <1234@host.domain>"
*/

/* responses to "takethis <id>.  Must include ID of article */

/* End of new entries for the "streaming" protocol */

/*
**  The first character of an NNTP reply can be used as a category class.
*/
#define NNTP_CLASS_OK			'2'
#define NNTP_CLASS_ERROR		'4'
#define NNTP_CLASS_FATAL		'5'


/*
**  Authentication commands from the RFC update (not official).
*/
#define NNTP_AUTH_NEEDED		"480"
#define NNTP_AUTH_BAD			"481"
#define NNTP_AUTH_NEXT			"381"
#define NNTP_AUTH_SASL_CHAL		"383"
#define NNTP_AUTH_OK			"281"
#define NNTP_AUTH_SASL_OK		"283"

/*
**  Starttls commands (not official).
*/
#define NNTP_STARTTLS_NEXT		"382"
#define NNTP_STARTTLS_DONE		"483"
#define NNTP_STARTTLS_BAD		"580"

/*
**  XGTITLE, from ANU news.
*/

/*
**  MODE CANCEL extension.
*/
#define NNTP_OK_CANCELLED       "289"

/*
**  XBATCH feed extension.
*/
#define NNTP_OK_XBATCHED	"239"
#define NNTP_CONT_XBATCH_STR	"339"
/* and one more meaning for the 436 code NNTP_FAIL_IHAVE_DEFER */
#define NNTP_RESENDIT_XBATCHERR	"436 xbatch failed: "
/* and one more meaning for the 501 code NNTP_SYNTAX_USE */
#define NNTP_XBATCH_BADSIZE	"501 Invalid or missing size for xbatch"

#define NNTP_STRLEN			512

/* Consensus on the USEFOR mailing list in June of 2000 indicates that the
   next revision of the Usenet article standard will limit the length of the
   message ID to 250 characters.  This is also the limit recommended by
   son-of-1036.

   You can increase this limit if you want, but don't increase it above 497.
   RFC 977 limits each line of the NNTP protocol to 512 octets, including
   the terminating CRLF.  For a message ID to be passed using the TAKETHIS
   command, it can therefore be a maximum of 501 octets.  The November 1999
   draft of the replacement RFC limits it to 497 octets.

   Both Cyclone and DNews are known to reject message IDs longer than 500
   octets as of June of 2000.  DNews has been reported to have problems with
   message IDs of 494 octets. */

#define NNTP_MSGID_MAXLEN       250
