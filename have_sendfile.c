/*
 * have_sendfile - determine if we have the sendfile system call
 *
 * @(#) $Revision: 1.1 $
 * @(#) $Id: have_sendfile.c,v 1.1 2003/03/06 08:24:07 chongo Exp $
 * @(#) $Source: /usr/local/src/bin/syncfile/RCS/have_sendfile.c,v $
 *
 * Copyright (c) 2003 by Landon Curt Noll.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright, this permission notice and text
 * this comment, and the disclaimer below appear in all of the following:
 *
 *       supporting documentation
 *       source copies
 *       source works derived from this source
 *       binaries derived from this source or from derived source
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * chongo (Landon Curt Noll) /\oo/\
 *
 * http://www.isthe.com/chongo/index.html
 * https://github.com/lcn2
 *
 * Share and enjoy!  :-)
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <string.h>


int
main(int argc, char *argv[])
{
    off_t offset = (off_t)0;	/* copy from the beginning */
    struct stat buf;		/* from file information */
    char *from;			/* from filename */
    int from_fd;		/* open file filename */
    char *to;			/* to filename */
    int to_fd;			/* open to filename */

    /* parse args */
    if (argc != 3) {
	fprintf(stderr, "usage: %s from to\n", argv[0]);
	exit(1);
    }
    from = argv[1];
    to = argv[2];

    /* open files */
    errno = 0;
    from_fd = open(from, O_RDONLY);
    if (from_fd < 0) {
	fprintf(stderr, "%s: cannot open %s for reading: %s\n",
		argv[0], from, strerror(errno));
	exit(2);
    }
    errno = 0;
    if (fstat(from_fd, &buf) < 0) {
	fprintf(stderr, "%s: cannot stat %s: %s\n",
		argv[0], to, strerror(errno));
	exit(3);
    }
    errno = 0;
    to_fd = open(to, O_WRONLY|O_CREAT|O_TRUNC, buf.st_mode);
    if (to_fd < 0) {
	fprintf(stderr, "%s: cannot open %s for writing: %s\n",
		argv[0], to, strerror(errno));
	exit(4);
    }

    /* sendfile from the from file to the to file :-) */
    errno = 0;
    if (sendfile(to_fd, from_fd, &offset, (size_t)(buf.st_size)) < 0) {
	fprintf(stderr, "%s: sendfile failed: %s\n",
		argv[0], strerror(errno));
	exit(5);
    }

    /* All done!  -- Jessica Noll, Age 2 */
    (void) close(from_fd);
    (void) close(to_fd);
    exit(0);
}
