#!/bin/make
# @(#)Makefile	1.2 04 May 1995 02:06:57
#
# syncfile - sync between two files
#
# @(#) $Revision: 1.1 $
# @(#) $Id: Makefile,v 1.1 2003/03/06 08:24:07 chongo Exp chongo $
# @(#) $Source: /var/tmp/syncfile/RCS/Makefile,v $
#
# Copyright (c) 2003 by Landon Curt Noll.  All Rights Reserved.
#
# Permission to use, copy, modify, and distribute this software and
# its documentation for any purpose and without fee is hereby granted,
# provided that the above copyright, this permission notice and text
# this comment, and the disclaimer below appear in all of the following:
#
#       supporting documentation
#       source copies
#       source works derived from this source
#       binaries derived from this source or from derived source
#
# LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
# EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
# USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
#
# chongo (Landon Curt Noll, http://www.isthe.com/chongo/index.html) /\oo/\
#
# Share and enjoy! :-)


SHELL= /bin/sh
CC= cc
CFLAGS= -O3 -g3
#CFLAGS= -g3

TOPNAME= cmd
INSTALL= install

DESTDIR= /usr/local/bin

TARGETS= syncfile

all: ${TARGETS}

syncfile: syncfile.c have_sendfile.h
	${CC} ${CFLAGS} syncfile.c -o syncfile

have_sendfile.h: have_sendfile.c Makefile
	-@rm -f have_sendfile.o have_sendfile have_sendfile.h tmp
	@echo 'forming have_sendfile.h'
	@echo '/*' > have_sendfile.h
	@echo ' * DO NOT EDIT -- generated by the Makefile' >> have_sendfile.h
	@echo ' */' >> have_sendfile.h
	@echo '' >> have_sendfile.h
	@echo '#if !defined(__HAVE_SENDFILE__)' >> have_sendfile.h
	@echo '#define __HAVE_SENDFILE__' >> have_sendfile.h
	@echo '' >> have_sendfile.h
	@echo '/* do we have the sendfile system call? */' >> have_sendfile.h
	-@${CC} ${CFLAGS} have_sendfile.c -o have_sendfile >/dev/null 2>&1;true
	-@rm -f tmp
	-@${SHELL} -c "./have_sendfile have_sendfile tmp >/dev/null 2>&1" \
	    >/dev/null 2>&1; true
	-@if cmp -s have_sendfile tmp; then \
	    echo '#define HAVE_SENDFILE /* yes we have the call */'; \
	else \
	    echo '#undef HAVE_SENDFILE /* no we do not have the call */'; \
	fi >> have_sendfile.h
	@echo '' >> have_sendfile.h
	@echo '#endif /* __HAVE_SENDFILE__ */' >> have_sendfile.h
	-@rm -f have_sendfile.o have_sendfile tmp
	@echo 'formed have_sendfile.h'

configure:
	@echo nothing to configure

clean quick_clean quick_distclean distclean:
	rm -f syncfile.o tmp

clobber quick_clobber: clean
	rm -f ${TARGETS}

install: all
	@${INSTALL} -m 0555 ${TARGETS} ${DESTDIR}
