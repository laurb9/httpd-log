#-----------------------------------------------------------------------------
# Copyright (C)2001-2003 InfoStreet, Inc.    www.infostreet.com
#
# Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
# Created:  Feb 1, 2001
# $LastChangedDate$
# $LastChangedBy$
# $Revision$
#
# Description:
# automake source file for Makefile
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# Version 2, as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to:
# Free Software Foundation, Inc. 
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
#-----------------------------------------------------------------------------

# Note that files are installed in /usr/local by default. 
# You need to change it by running configure --prefix=<path>

bin_PROGRAMS        = httpd-logger
sbin_PROGRAMS       = httpd-logd
httpd_logd_SOURCES  = logserver.c log_entry.c fd_cache.c
httpd_logd_LDADD    = $(LIBOBJS) -L. -lcore
httpd_logger_SOURCES= logger.c
httpd_logger_LDADD  = $(LIBOBJS) -L. -lcore

noinst_LIBRARIES    = libcore.a
libcore_a_SOURCES   = debug.c hash.c signalnames.c
 
debug.c: debug.h
hash.c:  hash.h
 
signalnames.c: makesignaldefs.pl
	perl ./$< > $@
 