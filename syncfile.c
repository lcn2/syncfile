/*
 * syncfile - sync between two files
 *
 * Copyright (c) 2003,2023,2025 by Landon Curt Noll.  All Rights Reserved.
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
 * Share and Enjoy!     :-)
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <utime.h>
#include <sys/time.h>

#include "have_sendfile.h"
#if defined(HAVE_SENDFILE)
#include <sys/sendfile.h>
#endif


/*
 * official version
 */
#define VERSION "1.6.1 2025-03-24"          /* format: major.minor YYYY-MM-DD */


/*
 * flags
 */
static int fork_flag = 0;	/* 1 ==> fork into background at start */
static int verbose = 0;		/* output verbose messages */
static int del_dest = 0;	/* 1 ==> delete dest is src file is gone */
static int del_src = 0;		/* 1 ==> delete src is dest file is gone */
static int trunc = 0;		/* 1 ==> touch/truncate instead deleting */
static int dest_2_src = 0;	/* 1 ==> copy dest to src if dest is newer */
static double interval = 60.0;	/* seconds between checks */
static int64_t count = 1;	/* number of checks, 0 ==> infinite */
static char *suffix = ".new";	/* suffix when forming a new dest file */
static char *src = NULL;	/* src sync file */
static char *dest = NULL;	/* dest sync file */
static uid_t uid;		/* 0 ==> we are the superuser, can chown */


/*
 * usage
 */
static char *program = NULL;		/* our name */
static char *prog = NULL;		/* basename of our name */
static const char * const usage =
    "usage: %s [-h] [-v] [-V] [-f] [-d] [-D] [-T] [-c] [-t secs] [-n cnt] [-s suffix] src dest\n"
    "\n"
    "\t-h\t   print this message\n"
    "\t-v\t   output progress messages to stdout\n"
    "\t-V\t   print version string and exit\n"
    "\n"
    "\t-f\t   fork into background\n"
    "\n"
    "\t-d\t   delete dest when src file does not exist\n"
    "\t-D\t   delete src when dest file does not exist\n"
    "\t-T\t   create/truncate files if one file is missing (conflicts with -d and -D)\n"
    "\n"
    "\t-c\t   copy dest to src if dest is newer or src is gone (def: don't)\n"
    "\n"
    "\t-t secs\t   check interval (may be a float) (def: 60.0)\n"
    "\t-n cnt\t   number of checks, 0 ==> infinite (def: 1)\n"
    "\n"
    "\t-s suffix  filename suffix when forming new files (def: .new)\n"
    "\n"
    "\tsrc\t   src file\n"
    "\tdest\t   destination file\n"
    "\n"
    "Exit codes:\n"
    "    0         all OK\n"
    "    2         -h and help string printed or -V and version string printed\n"
    "    3         command line error\n"
    " >= 10        internal error\n"
    "\n"
    "%s version: %s\n";


/*
 * forward declarations
 */
static void pr_usage(FILE *stream);
static void parse_args(int argc, char *argv[]);
static void dsleep(double timeout);
static void debug(char *fmt, ...);
static void copy_file(int from_fd, struct stat *src_buf,
		      char *from, char *new_to, char *to);


int
main(int argc, char *argv[])
{
    pid_t pid;			/* pid of child or 0 (parent) or < 0 (error) */
    int64_t cycle_num = 0;	/* next cycle number */

    struct stat src_buf;	/* src status */
    int src_exists;		/* 1 ==> src exists, 0 ==> missing */
    int src_fd = -1;		/* open src descriptor or -1 => no file */
    char *new_src = NULL;	/* src.new temp filename */

    struct stat dest_buf;	/* dest status */
    int dest_exists;		/* 1 ==> dest exists, 0 ==> missing */
    int dest_fd = -1;		/* open dest descriptor or -1 => no file */
    char *new_dest = NULL;	/* dest.new temp filename */

    /*
     * parse args
     */
    program = argv[0];
    parse_args(argc, argv);
    uid = geteuid();
    if (verbose) {
	debug("sync from: %s", src);
	debug("sync to: %s", dest);
	debug("check interval: %f sec", interval);
	debug("number of checks: %d", count);
	if (trunc) {
	    debug("truncate dest if src is missing: %d", del_dest);
	    debug("truncate src if dest is missing: %d", del_src);
	} else {
	    debug("delete dest if src is missing: %d", del_dest);
	    debug("delete src if dest is missing: %d", del_src);
	}
	debug("new dest file suffux: %s", suffix);
	if (uid == 0) {
	    debug("will also set ownership and group of file");
	}
    }

    /*
     * I/O cleanup
     */
    fclose(stdin);

    /*
     * fork into background if needed
     */
    if (fork_flag) {

	/* fork me :-) */
	debug("forking into background, debug disabled on child");
	errno = 0;
	pid = fork();
	if (pid < 0) {
	    /* bad fork */
	    fprintf(stderr, "%s: fork failed: %s\n", program, strerror(errno));
	    exit(10); /*coo*/
	} else if (pid > 0) {
	    /* parent code */
	    debug("forked pid: %d, parent exiting", pid);
	    exit(0); /*ooo*/
	}

	/* child code from now on */
	verbose = 0;
    }

    /*
     * form temp filenames
     */
    new_src = (char *)malloc(strlen(src) + strlen(suffix) + 1);
    if (new_src == NULL) {
	fprintf(stderr, "%s: new_src malloc failed\n", program);
	exit(11);
    }
    sprintf(new_src, "%s%s", src, suffix);
    new_dest = (char *)malloc(strlen(dest) + strlen(suffix)) + 1;
    if (new_dest == NULL) {
	fprintf(stderr, "%s: new_dest malloc failed\n", program);
	exit(12);
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
	    if (interval > 0.0) {
		debug("sleeping for %f seconds", interval);
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
	 * cannot move the file between a stat and open.  We also
	 * use sendfile which needs at least the src file descriptor.
	 */
	src_fd = open(src, O_RDWR);
	if (src_fd < 0) {
	    if (access(src, F_OK) == 0) {
		debug("src exists but is not readable: %s", src);
		continue;
	    } else {
		/* no such file */
		src_exists = 0;
		memset(&src_buf, 0, sizeof(src_buf));
		debug("src file is missing: %s", src);
	    }
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
	dest_fd = open(dest, O_RDWR);
	if (dest_fd < 0) {
	    if (access(dest, F_OK) == 0) {
		debug("dest exists but is not readable: %s", dest);
		continue;
	    } else {
		/* no such file */
		dest_exists = 0;
		memset(&dest_buf, 0, sizeof(dest_buf));
		debug("dest file is missing: %s", dest);
	    }
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

	/* nothing to do if both files are missing, unless -T */
	if (!src_exists && !dest_exists) {
	    debug("both src and dest are missing");
	    continue;
	}

	/* ignore if any existing file is NOT a regular file */
	if (src_exists && !S_ISREG(src_buf.st_mode)) {
	    debug("src: %s is not a regular file", src);
	    continue;
	}
	if (dest_exists && !S_ISREG(dest_buf.st_mode)) {
	    debug("dest: %s is not a regular file", dest);
	    continue;
	}

	/* deal with a missing src file */
	if (!src_exists) {

	    /* remove dest if src is missing and -d */
	    if (del_dest) {
		debug("src is missing and -d was given");
		errno = 0;
		if (unlink(dest) < 0) {
		    debug("unable to remove dest: %s: %s",
			  dest, strerror(errno));
		} else {
		    debug("removed dest: %s", dest);
		}

	    /* touch / truncate both files if -T (src is missing) */
	    } else if (trunc) {
		if (ftruncate(dest_fd, (off_t)0) < 0) {
		    debug("unable to truncate dest: %s: %s",
			  dest, strerror(errno));
		} else {
		    debug("truncated dest: %s", dest);
		    errno = 0;
		    src_fd = open(src, O_RDWR|O_CREAT|O_TRUNC,
				  dest_buf.st_mode);
		    if (src_fd < 0) {
			debug("unable to create empty src: %s: %s",
			      src, strerror(errno));
		    } else {
			debug("created empty src: %s", src);
		    }
		}

	    /* no src and no -d and no -T, so nothing to do */
	    } else {
		debug("src is missing");
	    }
	    continue;
	}

	/* deal with a missing dest file */
	if (!dest_exists) {

	    /* remove src if dest is missing and -D */
	    if (del_src) {
		debug("dest is missing and -D was given");
		errno = 0;
		if (unlink(src) < 0) {
		    debug("unable to remove src: %s: %s",
			  src, strerror(errno));
		} else {
		    debug("removed src: %s", src);
		}

	    /* touch / truncate both files if -T and dest is missing */
	    } else if (trunc) {
		if (ftruncate(src_fd, (off_t)0) < 0) {
		    debug("unable to truncate src: %s: %s",
			  src, strerror(errno));
		} else {
		    debug("truncated src: %s", src);
		    errno = 0;
		    dest_fd = open(dest, O_RDWR|O_CREAT|O_TRUNC,
				   src_buf.st_mode);
		    if (dest_fd < 0) {
			debug("unable to create empty dest: %s: %s",
			      dest, strerror(errno));
		    } else {
			debug("created empty dest: %s", dest);
		    }
		}

	    /* no dest and no -D and no -T, so nothing to do */
	    } else {
		debug("dest is missing");
	    }
	    continue;
	}

	/* copy src to dest if dest is missing and no -D */
	if (!dest_exists && !del_src) {
	    debug("src: %s exists and dest: %s is missing", src, dest);
	    copy_file(src_fd, &src_buf, src, new_dest, dest);
	    continue;
	}

	/* copy dest to src if src is missing and -b and no -d */
	if (!src_exists && dest_2_src && !del_dest) {
	    debug("dest: %s exists and src: %s is missing", dest, src);
	    copy_file(dest_fd, &dest_buf, dest, new_src, src);
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
		copy_file(dest_fd, &dest_buf, dest, new_src, src);
	    } else {
		debug("copying to src: %s to dest: %s", src, dest);
		copy_file(src_fd, &src_buf, src, new_dest, dest);
	    }
	    continue;
	}

	/* src and dest must be identical or similar */
	debug("src and dest look similar");

    } while (count == 0 || cycle_num < count);

    /*
     * all done!  -- Jessica Noll, Age 2
     */
    exit(0); /*ooo*/
}


/*
 * pr_usage - print usage message
 *
 * given:
 *
 *    stream - print usage message on stream, NULL ==> stderr
 */
static void
pr_usage(FILE *stream)
{
    /*
     * NULL stream means stderr
     */
    if (stream == NULL) {
        stream = stderr;
    }

    /*
     * firewall - change program if NULL
     */
    if (program == NULL) {
        program = "((NULL))";
    }

    /*
     * firewall set name if NULL
     */
    if (prog == NULL) {
        prog = rindex(program, '/');
    }
    /* paranoia if no / is found */
    if (prog == NULL) {
        prog = program;
    } else {
	++prog;
    }

    /*
     * print usage message to stderr
     */
    fprintf(stream, usage, program, prog, VERSION);
}


/*
 * parse_args - parse command args
 *
 * given:
 *	argc	number of args to parse
 *	argv	command arg list
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
    while ((i = getopt(argc, argv, "hvVfdDTct:n:s:")) != -1) {
	switch (i) {
	case 'h':	/* print help message */
	    pr_usage(stderr);
	    exit(2); /*ooo*/
	    /*NOTREACHED*/
	case 'v':	/* verbose output */
	    verbose = 1;
	    break;
	case 'V':	/* verbose output */
	    printf("%s\n", VERSION);
	    exit(2); /*ooo*/
	    /*NOTREACHED*/
	case 'f':	/* fork info background */
	    fork_flag = 1;
	    break;
	case 'd':	/* delete dest when src file does not exist */
	    del_dest = 1;
	    break;
	case 'D':	/* delete src when dest file does not exist */
	    del_src = 1;
	    break;
	case 'T':	/* truncate/touch both files of one file is missing */
	    trunc = 1;
	    break;
	case 'c':	/* cp dest to src if dest is newer */
	    dest_2_src = 1;
	    break;
	case 't':
	    errno = 0;
	    interval = strtod(optarg, NULL);
	    if (errno == ERANGE) {
		fprintf(stderr, "%s: invalid -t interval value\n", program);
		exit(3); /*ooo*/
		/*NOTREACHED*/
	    } else if (interval <= 0.0) {
		fprintf(stderr,
			"%s: -t interval value must be > 0.0\n", program);
		exit(3); /*ooo*/
		/*NOTREACHED*/
	    }
	    break;
	case 'n':
	    errno = 0;
	    count = strtoll(optarg, NULL, 0);
	    if (errno == ERANGE) {
		fprintf(stderr, "%s: invalid -n count value\n", program);
		exit(3); /*ooo*/
		/*NOTREACHED*/
	    } else if (count < 0) {
		fprintf(stderr, "%s: -n count must be >= 0\n", program);
		exit(3); /*ooo*/
		/*NOTREACHED*/
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
		    exit(3); /*ooo*/
		    /*NOTREACHED*/
		}
	    }
	    break;
	default:
	    pr_usage(stderr);
	    exit(3); /*ooo*/
	    /*NOTREACHED*/
	}
    }
    if (trunc && (del_dest || del_src)) {
	fprintf(stderr, "%s: -T conflicts with -d and -D", program);
	exit(3); /*ooo*/
	/*NOTREACHED*/
    }

    /*
     * parse flags
     */
    if (optind+2 != argc) {
	fprintf(stderr, "%s: required to args are missing\n", program);
	pr_usage(stderr);
	exit(3); /*ooo*/
	/*NOTREACHED*/
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
 *	timeout		seconds to sleep as a float
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
 *	fmt	printf-like format of the main part of the debug message
 *	...	optional debug message args
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
	fprintf(stdout, "%s:%f: ",
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
 *	from_fd		open file descriptor to copy from
 *	src_buf		pointer to fstat of from_fd
 *	from		name of file being copied from
 *	new_to		temp filename in same directory as to
 *	to		filename being copied into
 *
 * We copy into a temp filename and then rename it to the destination.
 * This means that the to file will never contain a partial copy
 * of the from file.  The to file will either have its original contents
 * or the contents of the from file ... nothing in between.
 *
 * This function also sets the modification time of the to file
 * to match the from file.
 */
static void
copy_file(int from_fd, struct stat *src_buf, char *from, char *new_to, char *to)
{
    int to_fd = -1;		/* new_to open file descriptor */
    size_t count;		/* bytes left to transfer */
    ssize_t written;		/* bytes written */
    struct utimbuf timebuf;	/* access and modification time to set */
#if defined(HAVE_SENDFILE)
    off_t offset = (off_t)0;	/* starting offset of transfer */
#else
    int readcnt;		/* bytes read from from */
    char buf[BUFSIZ+1];		/* I/O buffer */
#endif

    /*
     * firewall
     */
    if (from_fd < 0) {
	fprintf(stderr, "%s: copy_file from_fd < 0: %d\n", program, from_fd);
	exit(13);
    }
    if (src_buf == NULL || from == NULL || new_to == NULL || to == NULL) {
	fprintf(stderr, "%s: called with NULL ptr\n", program);
	exit(14);
    }

    /*
     * open the temp from file
     */
    debug("opening temp file: %s", new_to);
    errno = 0;
    to_fd = open(new_to, O_CREAT|O_EXCL|O_TRUNC|O_WRONLY, src_buf->st_mode);
    if (to_fd < 0) {
	debug("unable to open temp file: %s: %s", new_to, strerror(errno));
	return;
    }

    /*
     * send data from the from file to the to file :-)
     */
    count = (off_t)src_buf->st_size;
    if (count > 0) {
	debug("copying %lld octets %s ==> %s", (long long)count, from, new_to);
	do {

#if defined(HAVE_SENDFILE)
	    /*
	     * transfer by sendfile
	     */
	    errno = 0;
	    written = sendfile(to_fd, from_fd, &offset, count);

	    /* transfer failed, EINTR is the only OK error */
	    if (written < 0 && errno != EINTR) {
		debug("sendfile %s to %s failed: %s",
		      from, new_to, strerror(errno));
		(void) close(to_fd);
		(void) unlink(new_to);
		return;
	    } else if (written == 0) {
		debug("sendfile transferred 0 octets");
		(void) close(to_fd);
		(void) unlink(new_to);
		return;
	    }

	    /* determine next count needed, if any */
	    count = src_buf->st_size - offset;

#else

	    /*
	     * transfer by read/write buffer
	     */
	    count = 0;
	    errno = 0;
	    /* read a buffer */
	    readcnt = read(from_fd, buf, BUFSIZ);
	    if (readcnt < 0) {
		/* EINTR is the only OK error */
		if (errno != EINTR) {
		    debug("bad read from %s: %s", from, strerror(errno));
		    (void) close(to_fd);
		    (void) unlink(new_to);
		    return;
		}
	    } else if (readcnt == 0) {
		debug("empty read from %s: %s", from, strerror(errno));
		(void) close(to_fd);
		(void) unlink(new_to);
		return;

	    /* write the same buffer */
	    } else if (readcnt > 0) {
		errno = 0;
		written = write(to_fd, buf, readcnt);
		if (written < 0) {
		    debug("bad write to %s: %s", new_to, strerror(errno));
		    (void) close(to_fd);
		    (void) unlink(new_to);
		    return;
		} else if (written != readcnt) {
		    debug("wrote only %d out of %s to %s: %s",
			  written, readcnt, new_to, strerror(errno));
		    (void) close(to_fd);
		    (void) unlink(new_to);
		    return;
		}
	    }

	    /* note how much data is left to transfer */
	    count -= written;

#endif

	} while (count > 0);

    } else {
	debug("src is empty, creating empty %s", new_to);
    }

    /*
     * set mode
     */
    errno = 0;
    if (fchmod(to_fd, src_buf->st_mode) < 0) {
	debug("cannot chmod %s %03o: %s",
	      new_to, src_buf->st_mode, strerror(errno));
	(void) unlink(new_to);
	return;
    }

    /*
     * set ownership and group if we are root
     */
    if (uid == 0 && fchown(to_fd, src_buf->st_uid, src_buf->st_gid) < 0) {
	debug("unable to chown %d.%d of %s: %s",
	      src_buf->st_uid, src_buf->st_gid, new_to,
	      strerror(errno));
	debug("will continue anyway");
	/* OK to continue */
    }

    /*
     * close up the complete and new file
     */
    (void) close(to_fd);

    /*
     * set new file attributes
     */
    timebuf.actime = src_buf->st_atime;
    timebuf.modtime = src_buf->st_mtime;
    errno = 0;
    if (utime(new_to, &timebuf) < 0) {
	debug("unable to set file time on %s: %s", new_to, strerror(errno));
	(void) unlink(new_to);
	return;
    }

    /*
     * move new file into place
     */
    debug("rename %s ==> %s", new_to, to);
    errno = 0;
    if (rename(new_to, to) < 0) {
	debug("move %s to %s failed: %s", new_to, to, strerror(errno));
	(void) unlink(new_to);
	return;
    }
    debug("completed sync %s ==> %s", from, to);
    return;
}
