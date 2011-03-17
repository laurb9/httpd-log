/*
 * Copyright (C)1999-2003 InfoStreet, Inc.    www.infostreet.com
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 26, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * Common constants and includes.
 * Generic logging support via stderr or syslog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <stdarg.h>

#include "config.h"
#ifdef USE_SYSLOG
# include <syslog.h>
# include <string.h>
void init_syslog(char *ident, int options);
#endif

#define DEBUG_DEFAULT 9
#define DEBUG_ERROR   0
#define DEBUG_MIN     1
#define DEBUG_MED     4
#define DEBUG_MAX     7

extern int gDebug;

#define LAST_ERROR strerror(errno)

#define DEBUG_ENABLE
/*
 * This defines the LOG_PRINTF macro so that debugging code is
 * only included when needed.
 * Use DIE_ERROR for reporting level 0 errors and die
 */
/*
  #ifdef DEBUG_ENABLE
  #define LOG_PRINTF(level, args) log_printf(level, args)
  #else
  #define LOG_PRINTF(args)
  #endif
*/

#define LOG_PRINTF log_printf
#define DIE_ERROR die_printf

#define ZONE __FILE__, __LINE__

void debug_init(char *ident, int debug_level, int options);
extern char *SIGNAL_NAME(int); /* defined in signalnames.c */

void log_printf(int level, char *file, int line, const char *format, ...);
void die_printf(int code, char *file, int line, const char*format, ...);

#endif
