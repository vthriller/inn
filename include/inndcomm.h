/*  $Revision$
**
**  Here be values used for communicating with the server once it is
**  running.
*/

/* The header for the ICC protocol is a one-byte protocol version followed
   by a 4 byte messages length*/
#define HEADER_SIZE (sizeof (ICC_PROTOCOLTYPE) + sizeof (ICC_MSGLENTYPE))

typedef short ICC_MSGLENTYPE;	/* Length code to prefix commands to
				** the server. */
typedef char ICC_PROTOCOLTYPE ;

/* Values for the protocol version field of the message. 8 bits wide. */
#define ICC_PROTOCOL_1 'a'



#define SC_SEP		'\001'
#define SC_MAXFIELDS	6

#define SC_ADDHIST	'a'
#define SC_ALLOW	'D'
#define SC_BEGIN	'b'
#define SC_CANCEL	'c'
#define SC_CHANGEGROUP	'u'
#define SC_CHECKFILE	'd'
#define SC_DROP		'e'
#define SC_FEEDINFO	'F'
#define SC_FILTER	'T'
#define SC_FLUSH	'f'
#define SC_FLUSHLOGS	'g'
#define SC_GO		'h'
#define SC_HANGUP	'i'
#define SC_LOGMODE	'E'
#define SC_LOWMARK	'L'
#define SC_MODE		's'
#define SC_NAME		'j'
#define SC_NEWGROUP	'k'
#define SC_PARAM	'l'
#define SC_PAUSE	'm'
#define SC_PERL		'P'
#define SC_READERS	'v'
#define SC_REJECT	'C'
#define SC_RELOAD	'o'
#define SC_RENUMBER	'n'
#define SC_RESERVE	'z'
#define SC_RMGROUP	'p'
#define SC_SEND		'A'
#define SC_SHUTDOWN	'q'
#define SC_STATUS	'S'
#define SC_SIGNAL	'B'
#define SC_THROTTLE	'r'
#define SC_TIMER        'Z'
#define SC_TRACE	'w'
#define SC_XABORT	'x'
#define SC_XEXEC	'y'

    /* Yes, we don't want anyone to use this. */
#define SC_FIRSTFREE	G

#define MAX_REASON_LEN 80


extern void	ICCsettimeout();
extern int	ICCopen();
extern int	ICCclose();
extern int	ICCcommand();
extern int	ICCcancel();
extern int	ICCgo();
extern int	ICCpause();
extern int	ICCreserve();

extern char	*ICCfailure;
