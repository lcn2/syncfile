/*
 * syncfile - sync between two files
 *
 * @(#) $Revision$
 * @(#) $Id$
 * @(#) $Source$
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
 * chongo (Landon Curt Noll, http://www.isthe.com/chongo/index.html) /\oo/\
 *
 * Share and enjoy! :-)
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>


/*
 * flags
 */
static int verbose = 0;		/* output verbose messages */
static int del_dest = 0;	/* 1 ==> delete dest is src file is gone */
static int del_src = 0;		/* 1 ==> delete src is dest file is gone */
static int chk_contents = 0;	/* 1 ==> also compare file contents */
static int dest_2_src = 0;	/* 1 ==> copy dest to src if dest is newer */
static double interval = 60.0;	/* seconds between checks */
static int64_t count = 1;	/* number of checks, 0 ==> infinite */
static char *suffix = ".new";	/* suffix when forming a new dest file */
static char *src = NULL;	/* src sync file */
static char *dest = NULL;	/* dest sync file */


/*
 * usage
 */
static char *program;		/* our name */
static char *cmdline =
    "[-h] [-v] [-d] [-D] [-c] [-b] [-t secs] [-n numtry] [-s suffix] src dest\n"
    "\n"
    "\t-h\t\tprint this message\n"
    "\t-v\t\toutput progress messages to stdout\n"
    "\n"
    "\t-d\t\tdelete dest when src file does not exist\n"
    "\t-D\t\tdelete src when dest file does not exist\n"
    "\n"
    "\t-c\t\tcompare file contents (def: only check mode, mod time, len)\n"
    "\t-b\t\tcopy dest to src if dest is newer or src is gone (def: don't)\n"
    "\n"
    "\t-t secs\tcheck interval (may be a float) (def: 60.0)\n"
    "\t-n cnt\tnumber of checks, 0 ==> infinite (def: 1)\n"
    "\n"
    "\t-s suffix\tfilename suffix when forming new files(def: .new)\n";


/*
 * forward declarations
 */
static void parse_args(int argc, char *argv[]);
static void dsleep(double timeout);
static void debug(char *fmt, ...);
static void copy_file(int dest_fd, char *dest, char *new_src, char *src);


int
main(int argc, char *argv[])
{
    char *new_src = NULL;	/* src.new temp filename */
    char *new_dest = NULL;		/* dest.new temp filename */
    int64_t cycle_num = 0;	/* next cycle number */

    struct stat src_buf;	/* src status */
    int src_exists;		/* 1 ==> src exists, 0 ==> missing */
    int src_fd = -1;		/* open src descriptor or -1 => no file */

    struct stat dest_buf;	/* dest status */
    int dest_exists;		/* 1 ==> dest exists, 0 ==> missing */
    int dest_fd = -1;		/* open dest descriptor or -1 => no file */

    /*
     * parse args
     */
    parse_args(argc, argv);
    if (verbose) {
	debug("sync from: %s to: %s", src, dest);
	debug("compare file contents: %d", chk_contents);
	debug("check interval: %f sec", interval);
	debug("number of checks: %d", count);
	debug("ok to delete dest: %d", del_dest);
	debug("ok to delete src: %d", del_src);
	debug("new dest file suffux: %s", suffix);
    }

    /*
     * form temp filenames
     */
    new_src = (char *)malloc(strlen(src) + strlen(suffix) + 1);
    if (new_src == NULL) {
	fprintf(stderr, "%s: new_src malloc failed\n", program);
	exit(1);
    }
    sprintf(new_src, "%s%s", src, suffix);
    new_src = (char *)malloc(strlen(dest) + strlen(suffix)) + 1;
    if (dest == NULL) {
	fprintf(stderr, "%s: new_dest malloc failed\n", program);
	exit(2);
    }
    sprintf(new_dest, "%s%s", dest, suffix);

    /*
     * sync cycle
     */
    debug("stating cycle 0");
    do {

	/* close any open files */
	if (src_fd >= 0) {
	    (void) close(src_fd);
	    src_fd = -1;
	}
	if (dest_fd >= 0) {
	    (void) close(dest_fd);
	    dest_fd = -1;
	}

	/* sleep if not first cycle */
	if (cycle_num > 0) {
	    if (inteveral > 0.0) {
		debug("sleeping for %f seconds", inteveral);
		dsleep(interval);
	    }
	    debug("stating cycle %lld", cycle_num);
	}
	++cycle_num;

	/*
	 * attempt to open both files
	 *
	 * We use open files because we can fstat the descriptor knowing
	 * that we are talking about the file that we opened.  I.e., someone
	 * cannot move the file between a stat and and open.  We also
	 * use sendfile which needs at least the src file descriptor.
	 */
	src_fd = open(src, O_RDONLY);
	if (src_fd < 0) {
	    /* no such file */
	    src_exists = 0;
	    memset(&src_buf, 0, sizeof(src_buf));
	    debug("src file is missing: %s", src);
	} else {
	    src_exists = 1;
	    if (fstat(src_fd, &src_buf) < 0) {
		/* stat filed, assume file does not exist */
		close(src_fd);
		src_exists = 0;
		memset(&src_buf, 0, sizeof(src_buf));
		debug("src fstat failed, assume it is missing: %s", src);
	    } else {
		debug("src file exists: %s", src);
	    }
	}
	dest_fd = dest(dest, O_RDWR);
	if (dest_fd < 0) {
	    /* no such file */
	    dest_exists = 0;
	    memset(&dest_buf, 0, sizeof(dest_buf));
	    debug("dest file is missing: %s", dest);
	} else {
	    dest_exists = 1;
	    if (fstat(dest_fd, &dest_buf) < 0) {
		/* stat filed, assume file does not exist */
		close(dest_fd);
		dest_exists = 0;
		memset(&dest_buf, 0, sizeof(dest_buf));
		debug("dest fstat failed, assume it is missing: %s", dest);
	    } else {
		debug("dest file exists: %s", dest);
	    }
	}


	/* nothing to do if both files are missing */
	if (!src_exists && !dest_exists) {
	    debug("both src and dest are missing, doing nothing");
	    continue;
	}

	/* ignore if any existing file is NOT a regular file */
	if (src_exists && !S_ISREG(src_buf.st_mode)) {
	    debug("src: %s is not a regular file, doing nothing", src);
	    continue;
	}
	if (dest_exists && !S_ISREG(dest_buf.st_mode)) {
	    debug("dest: %s is not a regular file, doing nothing", dest);
	    continue;
	}

	/* remove dest if src is missing and -d */
	if (!src_exists && del_dest) {
	    debug("src is missing and -d was given");
	    if (unlink(dest) < 0) {
		debug("unable to remove dest: %s", dest);
	    } else {
		debug("removed dest: %s", dest);
	    }
	    continue;
	}

	/* remove src if dest is missing and -D */
	if (!dest_exists && del_src) {
	    debug("dest is missing and -D was given");
	    if (unlink(dest) < 0) {
		debug("unable to remove src: %s", src);
	    } else {
		debug("removed src: %s", src);
	    }
	    continue;
	}

	/* copy src to dest if dest is missing and no -D */
	if (!dest_exists && !del_src) {
	    debug("src: %s exists and dest: %s is missing", src, dest);
	    copy_file(src_fd, src, new_dest, dest);
	    continue;
	}

	/* copy dest to src if src is missing and -b and no -d */
	if (!src_exists && dest_2_src && !del_dest) {
	    debug("dest: %s exists and src: %s is missing", dest, src);
	    copy_file(dest_fd, dest, new_src, src);
	    continue;
	}

	/* different modes, lengths, or mod times means we copy something */
	if (src_exists && dest_exists &&
	    (src_buf.st_mode != dest_buf.st_mode ||
	     src_buf.st_size != dest_buf.st_size ||
	     src_buf.st_mtime != dest_buf.st_mtime)) {

	    /* -b means we copy dest to src if dest is newer */
	    debug("src: %s and dest: %s are different", src, dest);
	    if (dest_2_src && src_buf.st_mtime < dest_buf.st_mtime) {
		debug("dest: %s is newer, copying to src: %s", dest, src);
		copy_file(dest_fd, dest, new_src, src);
	    } else {
		debug("copying to src: %s to dest: %s", src, dest);
		copy_file(src_fd, src, new_dest, dest);
	    }
	    continue;

	/* same mode,len,mod_time and -a means we compare and copy if diff */
	} else if (src_exists && dest_exists && chk_contents) {
	    debug("src: %s and dest: %s look similar, comparing contents",
		  src, dest);
	    if (contents_eq(src_fd, dest_fd)) {
		if (dest_2_src && src_buf.st_mtime < dest_buf.st_mtime) {
		    debug("dest: %s is newer, copying to src: %s", dest, src);
		    copy_file(dest_fd, dest, new_src, src);
		} else {
		    debug("copying to src: %s to dest: %s", src, dest);
		    copy_file(src_fd, src, new_dest, dest);
		}
	    }
	    continue;
	}

	/* src and dest must be identical or similar */
	if (chk_contents) {
	    debug("src: %s and dest: %s are identical, doing nothing",
		  src, dest);
	} else {
	    debug("src: %s and dest: %s are similar, doing nothing",
		  src, dest);
	}

    } while (count == 0 || cycle_num < count);

    /*
     * all done!  -- Jessica Noll, Age 2
     */
    exit(0);
}


/*
 * parse_args - parse command args
 *
 * given:
 * 	argc	number of args to parse
 * 	argv	command arg list
 */
static void
parse_args(int argc, char *argv[])
{
    extern char *optarg;	/* option argument */
    extern int optind;		/* argv index of the next arg */
    char *p;
    int i;

    /*
     * parse command flags
     */
    program = argv[0];
    while ((i = getopt(argc, argv, "hvdDcbt:n:s:")) != -1) {
	switch (i) {
	case 'h':	/* print help message */
	    fprintf(stderr, "usage: %s %s\n", program, cmdline);
	    exit(0);
	    /*NOTREACHED*/
	case 'v':	/* verbose output */
	    verbose = 1;
	    break;
	case 'd':	/* delete dest when src file does not exist */
	    del_dest = 1;
	    break;
	case 'D':	/* delete src when dest file does not exist */
	    del_src = 1;
	    break;
	case 'c':	/* compare file contents */
	    chk_contents = 1;
	    break;
	case 'b':	/* cp dest to src if dest is newer */
	    dest_2_src = 1;
	    break;
	case 't':
	    errno = 0;
	    interval = strtod(optarg, NULL);
	    if (errno == ERANGE) {
		fprintf(stderr, "%s: invalid -t interval value\n", program);
		exit(1);
	    } else if (interval <= 0.0) {
		fprintf(stderr,
			"%s: -t interval value must be > 0.0\n", program);
		exit(2);
	    }
	    break;
	case 'n':
	    errno = 0;
	    count = strtoll(optarg, NULL, 0);
	    if (errno == ERANGE) {
		fprintf(stderr, "%s: invalid -n count value\n", program);
		exit(3);
	    } else if (count < 0) {
		fprintf(stderr, "%s: -n count must be >= 0\n", program);
		exit(4);
	    }
	    break;
	case 's':	/* new file suffix */
	    suffix = optarg;
	    for (p=suffix; *p; ++p) {
		if (!isascii(*p) || (!isalnum(*p) && *p != '.' && *p != '_' &&
		    *p != '+' && *p != ',' && *p != '-')) {
		    fprintf(stderr,
			    "%s: -s suffux must only be [A-Za-z0-9._+,-]\n",
			    program);
		    exit(5);
		}
	    }
	    break;
	default:
	    fprintf(stderr, "usage: %s %s\n", program, cmdline);
	    exit(6);
	}
    }

    /*
     * parse flags
     */
    if (optind+2 != argc) {
	fprintf(stderr, "%s: required to args are missing\n", program);
	fprintf(stderr, "usage: %s %s\n", program, cmdline);
	exit(7);
    } else {
	src = argv[optind];
	dest = argv[optind+1];
    }
    return;
}


/*
 * dsleep - sleep for a double number of seconds
 *
 * given:
 * 	timeout		seconds to sleep as a float
 */
static void
dsleep(double timeout)
{
    struct timespec delay;	/* time to sleep */
    struct timespec remaining;	/* time left to sleep */

    /*
     * setup to sleep
     */
    delay.tv_sec = (time_t)timeout;
    delay.tv_nsec = (long)((timeout - (double)(delay.tv_sec)) * 1000000000.0);
    remaining.tv_sec = (time_t)0;
    remaining.tv_nsec = (long)0;

    /*
     * sleep until finished
     */
    do {
	/* sleep for the specified time */
	errno = 0;
	if (nanosleep(&delay, &remaining) < 0) {
	    delay = remaining;
	    remaining.tv_sec = (time_t)0;
	    remaining.tv_nsec = (long)0;
	}
    } while (errno == EINTR);
    return;
}


/*
 * debug - print debug message (if -v) to stdout
 *
 * given:
 * 	fmt	printf-like format of the main part of the debug message
 * 	...	optional debug message args
 */
static void
debug(char *fmt, ...)
{
    struct timeval now;		/* the current time */
    va_list ap;		/* argument pointer */

    /* only output if verbose (-v) */
    if (verbose) {

	/* output debug header */
	(void) gettimeofday(&now, NULL);
	fprintf(stdout, "%s: debug: %f: ",
		program,
		(double)now.tv_sec + ((double)now.tv_usec / 1000000.0));

	/* start the var arg setup and fetch our first arg */
	va_start(ap, fmt);

	/* output debug message */
	vfprintf(stdout, fmt, ap);

	/* clean up stdarg stuff */
	va_end(ap);

	/* output end of debug message */
	fputc('\n', stdout);
	fflush(stdout);
    }
    return;
}


/*
 * copy_file - copy from one file to another in a safe atomic fashion
 *
 * given:
 * 	from_fd		open file descriptor to copy from
 * 	from		name of file being copied from
 * 	new_to		temp filename in same directory as to
 * 	to		filename being copied into
 *
 * We copy into a temp filename and then rename it to the destination.
 * This means that the to file will never contain a partial copy
 * of the from file.  The to file will either have its origianl contents
 * or the contents of the from file ... nothing inbetween.
 *
 * This function also sets the modification time of the to file
 * to match the from file.
 */
static void
copy_file(int from_fd, char *from, char *new_to, char *to)
{
}
