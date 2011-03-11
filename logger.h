/*
 * Copyright (C)1999-2002 InfoStreet, Inc.    www.infostreet.com
 * Copyright (C)2009-2011 Laurentiu Badea     sourceforge.net/users/wotevah
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 23, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * Common constants and includes.
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

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#ifndef DEFAULT_DETACH
#define DEFAULT_DETACH 0 /* whether to fork processes in background or not */
#endif

/*
 * Host and (UDP) port for logging server. These defaults can be overridden
 * on command line
 */
#ifndef LOGGER_PORT
#define LOGGER_PORT 8181
#endif
#ifndef LOGGER_HOST
#define LOGGER_HOST "127.0.0.1" /* used only by log client */
#endif

/*
 * buffer sizes
 */
#define HOSTNAME_SIZE 128
#define MSG_SIZE 2048
#define PATH_SIZE 128

/*
 * Number of entries in the log spool area. The bigger the better
 * (depending on machine, after a certain number there is no improvement).
 * After this many entries are filled (or LOG_TIMEOUT occurrs) the
 * server spawns a child to dump the log entries in the right places.
 */
#ifndef LOG_ENTRIES
#define LOG_ENTRIES 128
#endif
/*
 * if no packets are received for LOG_TIMEOUT seconds, flush the log buffer.
 */
#define LOG_TIMEOUT 4
#define LOG_FIELD_SEPARATOR '\t' /* in apache log line */
/*
 * maximum number of simmultaneous live children allowed
 * (after this value is reached the server refuses new data
 */
#ifndef MAX_LIVE_CHILDREN
#define MAX_LIVE_CHILDREN 10
#endif
/*
 * delay time limit (when fork() fails a delay is inserted, which
 * starts at 1s and is doubled for every failure). This limits the
 * maximum wait time (in seconds).
 */
#ifndef MAX_FAILEDFORK_DELAY
#define MAX_FAILEDFORK_DELAY 64
#endif
/*
 * Log entry structure. All the char* fields are supposed to point
 * to somewhere inside the logline buffer, so that we don't need to
 * worry about fragmentation
 */
typedef struct {
    time_t time;        /* timestamp of request                */
    char *hostip;       /* source IP of request (%a)           */
    char *remote_user;  /* identd remote user   (%l)           */
    char *user;         /* authenticated user   (%u)           */
    unsigned status;    /* status of request    (%s)           */
    unsigned bytes;     /* bytes sent in reply  (%b)           */
    char *vhost;        /* virtual host name    (%v)           */
    char *user_agent;
    char *referrer;
    char *method;       /* GET POST or whatever                */
    char *uri;          /* URI of request                      */
    char *proto;        /* protocol of request (HTTP/1.1,etc)  */
    char logline[MSG_SIZE+1];    /* original log line        */
} log_entry;

#define LOGGER_SPOOL    "/var/log/httpd-log"

/* log file naming, in strftime(3) format TODO: this should be an option */
#define LOG_FILE_FORMAT "%Y-%m-%d.log" /* daily log */
/* #define LOG_FILE_FORMAT "%Y-%m.log" *//* monthly log */
/* #define LOG_FILE_FORMAT "%H:%M:%S.log" *//* every sec (debugging!) */
/*
 * update log file name every so often (secs)
 * do not define it to enable update only at midnight
 */
/* #define DEBUG_UPDATELOGFILE 0 */

#define TIMESTAMP_SIZE 29
/* strftime(3) format */
#define TIMESTAMP_FORMAT "[%d/%b/%Y:%H:%M:%S "
/* if APACHE_TZ is defined, the timestamp will get "+/-timezone]" appended */
#define APACHE_TZ
/* if LOG_EXTENDED is defined, log in NCSA extended log format */
#define LOG_EXTENDED
/*
 * We could inline these
 */
void get_hash(char hashed[PATH_SIZE], char *name);
int make_hash(char hashed[PATH_SIZE], char *name);
void process_entry(log_entry *rec);

void write_log_process(int p[2]); /* argument is pipe */

#endif
