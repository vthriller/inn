/*  $Revision$
**
**  Minimal <sys/uio.h> file for systems without one.
*/


struct iovec {
    char	*iov_base;
    int		iov_len;
};
