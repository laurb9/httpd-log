/*
 * Copyright (C)1999 InfoStreet, Inc.    www.infostreet.com
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 24, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * File Descriptors Cache module definitions.
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

#ifndef __FD_CACHE_H__
#define __FD_CACHE_H__

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include "logger.h" /* for PATH_SIZE */

/* What percentage of the fds have to be deleted from table on GarbageCol */
#define GC_DELETE 20 /* 20% */

typedef struct {
  int fd;
  time_t time;
  char file[PATH_SIZE];
} fd_element;

/*
 * Exported functions from this module
 */

/*
 * Initialize descriptors data structures. Call this first.
 */
void init_fd_table( void );
/*
 * Deallocate descriptors. Called automatically at program exit.
 */
void destroy_fd_table(void);
/*
 * Close all descriptors older than age and optionally flush buffers (sync(2)).
 */
void close_fd_all(int sync, time_t age);
/*
 * Do garbage collection. Argument is fractional, subunitary, specifies
 * how much to delete (0.1 for 10%, 1 for all). Use 0 to use default
 * defined above (GC_DELETE)
 */
void garbage_collect( int gc_delete );
/*
 * Retrieve the file descriptor corresponding to filename.
 * It opens the file if needed
 * Returns 0 if file could not be opened
 */
int get_fd( char *filename );

#endif
