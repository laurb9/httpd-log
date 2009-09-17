/*
 * Copyright (C)1999-2003 InfoStreet, Inc.    www.infostreet.com
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 24, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * File Descriptors Cache module.
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

static const char *VERSION = "$Id$";

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "fd_cache.h"
#include "debug.h"

fd_element **fd_array = NULL; /* main descriptors table                */
fd_element **fd_sort_array = NULL;/* descriptors table used for sorting
 (it holds the same pointers except the
 order may change)                     */
int fd_num, fd_allocated; /* fd_allocated starts at 0, upto fd_num */
void *mem_pool;

extern int debug;
extern int detached;

/*
 * Delete a file descriptor (+ name) from the list
 * Should be inlined.
 */
inline int delete_fd(int index) {
    /* This is a critical zone (in case we plan to multithread) */
    if (!fd_array[index]->fd) {
        return fd_allocated;
    }
    close(fd_array[index]->fd);
    fd_array[index]->fd = 0;
    fd_allocated--;
    if (DEBUG_MAX) {
        LOG_PRINTF(DEBUG_MAX, ZONE, "delete_fd(%d, \"%s\"): %d remaining.",
                   index, fd_array[index]->file, fd_allocated);
    }
    return fd_allocated;
}

/*
 * delete descriptor array, close everything.
 */
void destroy_fd_table(void) {
    int i;
    /*
     * Make sure all files are closed
     */
    LOG_PRINTF(DEBUG_MED, ZONE, "destroy_fd_table(): called.");

    if (fd_array) {
        for (i = 0; i < fd_num; i++) {
            delete_fd(i);
        }
    }
    free(fd_array);
    fd_array = NULL;
    free(fd_sort_array);
    fd_sort_array = NULL;
    free(mem_pool);
    mem_pool = NULL;

    LOG_PRINTF(DEBUG_MED, ZONE,
               "destroy_fd_table(): data structures deallocated.");
}

/*
 * Close and reinitialize all descriptors older than age (unix time)
 * and optionally flush buffers
 */
void close_fd_all(int do_sync, time_t age) {
    int i;
    LOG_PRINTF(1, ZONE,
               "close_fd_all(): closing all descriptors older than %d (total is %d).",
               age, fd_allocated);

    if (fd_array) {
        for (i = 0; i < fd_num; i++) {
            if (fd_array[i]->time < age) {
                delete_fd(i);
            }
        }
    }
    if (do_sync) {
        LOG_PRINTF(DEBUG_MED, ZONE, "close_fd_all(): flushing buffers to disk.");
        sync();
        LOG_PRINTF(DEBUG_MAX, ZONE, "close_fd_all(): sync complete.");
    }
}

/*
 * Initialize descriptor array
 */
void init_fd_table(void) {
    int i;
    static int flag = 1;

    LOG_PRINTF(DEBUG_MED, ZONE, "init_fd_table(): initializing fd_table.");

    fd_allocated = 0;
    fd_num = getdtablesize();
    fd_num = fd_num * 0.8; /* leave 20% for other uses */

    mem_pool = malloc(fd_num * sizeof(fd_element));
    fd_array = (fd_element**) malloc(fd_num * sizeof(fd_element));
    fd_sort_array = (fd_element**) malloc(fd_num * sizeof(fd_element));

    if (!mem_pool || !fd_array || !fd_sort_array) {
        DIE_ERROR(6, ZONE, "could not allocate descriptor pool");
    }

    /*
     * Don't register this multiple times if not needed
     */
    if (flag) {
        atexit(destroy_fd_table);
        flag = 0;
    }

    LOG_PRINTF(DEBUG_MIN, ZONE,
               "init_fd_table(): %d cells (%d+2*%d bytes) allocated.",
               fd_num, fd_num * sizeof(fd_element), fd_num * sizeof(fd_element*));

    /*
     * Now distribute chunks of the pool to individual elements
     */
    for (i = 0; i < fd_num; i++) {
        fd_array[i] = mem_pool + i * sizeof(fd_element);
        fd_array[i]->fd = 0; /* mark as unallocated */
        fd_sort_array[i] = fd_array[i];
    }
}

/*
 * All the functions below this assume that init_fd_table has been called.
 * There's no telling what can happen if they are wrong.
 */

/*
 * Comparison function for qsort()
 * use with:
 * qsort( fd_sort_array, fd_num, sizeof(fd_element*), compare_fd )
 */
int compare_fd(const void *a, const void *b) {
    return (((fd_element*) a)->time - ((fd_element*) b)->time);
}

/*
 * The hash function uses the sum of all characters modulo fd_num
 * if that slot is used, the next slots are tried in sequence.
 * Unused slots are marked with fd = 0
 */
inline int get_fd_hash(char *filename) {
    int hash;
    for (hash = 0; *filename; filename++) {
        hash += *filename;
    }
    return (hash % fd_num);
}

/*
 * Garbage collector. Deallocate least used descriptors;
 * gc_delete may be 0 to use default.
 * It is a fractional number telling how much to delete (100 to delete all)
 */
void garbage_collect(int gc_delete) {
    int count;

    if (!gc_delete) {
        gc_delete = GC_DELETE;
    }

    /*
     * (optional) only do deallocation if table is full
     */
    if (fd_allocated < fd_num) {
        return;
    }

    LOG_PRINTF(DEBUG_MAX, ZONE, "garbage_collect(): called, sorting array.");

    qsort(fd_sort_array, fd_num, sizeof(fd_element*), compare_fd);
    /*
     * Note that changing the values pointed to by fd_sort_array
     * will also change the fd_array ones since they share the same pointers
     */
    count = fd_num * gc_delete / 100;
    for (count--; count >= 0; count--) {
        delete_fd(count);
    }

    LOG_PRINTF(DEBUG_MED, ZONE, "garbage_collect(): finished, %d deleted.",
               fd_num * gc_delete / 100);
}

/*
 * Add a new file descriptor (+ name) to the list
 * Returns index in fd_array table
 */
int add_fd(int fd, char *filename) {
    int pos, i, count;
    pos = get_fd_hash(filename);

    /*
     * Look for an empty slot
     */
    for (i = pos, count = fd_num;
         count && fd_array[i]->fd;
         i = (i + 1) % fd_num, count--)
        ;
    if (count) {
        /*
         * Found an empty slot, fill it up.
         * This is a critical zone (in case we plan to multithread)
         */
        fd_array[i]->time = time(NULL);
        memcpy(fd_array[i]->file, filename, strlen(filename));
        fd_array[i]->fd = fd;

        fd_allocated++;

        LOG_PRINTF(DEBUG_MAX, ZONE,
                   "add_fd(%d, \"%s\"): hash %d, allocated %d.", fd, filename,
                   pos, i);

        return i;
    };
    /*
     * This should only happen if fd_allocated is 0
     */
    LOG_PRINTF(DEBUG_MED, ZONE, "add_fd(%d, \"%s\"): table full.", fd, filename);

    garbage_collect(0);
    return add_fd(fd, filename); /* let's be recursive */
}

/*
 * Make sure that specified path exists (by creating its elements eventually)
 * the path is in the format aaa/bbb/ccc/something
 * the file mentioned is not created (is ignored).
 */
int make_path(char *path) {
    char *pos;
    static struct stat stat_buf;
    int length, left;

    length = left = strlen(path);
    pos = path;

    while ((pos = memchr(pos, '/', left))) {
        *pos = '\0';
        if (!stat(path, &stat_buf)) {
            if (!S_ISDIR(stat_buf.st_mode)) {
                DIE_ERROR(7, ZONE, "%s is not a directory!", path);
            }
        } else {
            if (mkdir(path, 0755)) {
                DIE_ERROR(7, ZONE, "mkdir( %s ): %s", path, LAST_ERROR);
            }
        }
        LOG_PRINTF(DEBUG_MAX, ZONE, "make_path: created dir %s", path);

        *pos++ = '/';
        left = length - (pos - path);
    }

    LOG_PRINTF(DEBUG_MED, ZONE, "make_path: done with %s", path);

    return 0;
}

/*
 * Receive a file name, get a descriptor
 * zero means descriptor not found and unable to open file either
 * This is the function that's supposed to be called from the outside.
 */
int get_fd(char *filename) {
    int pos, i, length, count, fd;
    static int pos_last = -1, pos_cache;

    pos = get_fd_hash(filename);
    if (fd_array[pos]->fd) {
        /*
         * If we have reached this position last time make a jump to
         * the position that was found previously. Chances are it's the
         * same file requested.
         */
        if (pos == pos_last) {
            pos = pos_cache;
        } else {
            pos_last = pos;
        }

        length = strlen(filename);
        /*
         * Scan from this point on to find my file. Chances are it's
         * right here.
         */
        for (i = pos, count = fd_num; count && memcmp(fd_array[i]->file,
                filename, length); i = (i + 1) % fd_num, count--)
            ;

        if (count) {
            /*
             * actualize hit
             */
            fd_array[i]->time = time(NULL);
            pos_cache = i;

            LOG_PRINTF(DEBUG_MAX, ZONE,
                       "get_fd(\"%s\"): returning %d (pos %d).", filename,
                       fd_array[i]->fd, i);

            return fd_array[i]->fd;
        }
    }
    /*
     * Couldn't find it in the list, try to open file now
     */
    count = 2;
    while (count) {
        if ((fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0644)) > 0) {
            pos_cache = add_fd(fd, filename);
            return fd;

        } else {
            if (errno == EMFILE || errno == ENFILE) {
                /*
                 * we should not reach here
                 */
                DIE_ERROR(7, ZONE, "get_fd(%s): open: %s", filename, LAST_ERROR);
            } else {
                if (--count) {
                    make_path(filename);
                } else {
                    DIE_ERROR(7, ZONE, "get_fd(%s): open: %s", filename, LAST_ERROR);
                }
            }
        }
    }
    pos_last = -1;
    return 0;
}
