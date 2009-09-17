/*
 * Copyright (C)1999-2003 InfoStreet, Inc.    www.infostreet.com
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 23, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * Logger client. Receive log message through pipe and send them as UDP
 * packets to log server. This is a very simple client. All the work is
 * done at the server side.
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
/*
 *
 * TODO: maybe buffering ?
 */

static const char *VERSION = "$Id$";

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include "logger.h"
#include "debug.h"

extern char *SIGNAL_NAME(int); /* defined in signalnames.c */

char host[HOSTNAME_SIZE];
char buffer[MSG_SIZE];
int sock;

int debug = DEBUG_DEFAULT;
int detach = 0; /* to keep debug.c happy */
long packets = 0;
long discarded = 0;
long failed = 0;

struct option longs[] = {
    {"host",required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"debug", required_argument, NULL, 'd'},
    {"unknown", 0, NULL, 0}
};

const char shorts[] = "h:p:d:m";
char me[32];

void cleanup(int sig) {
    static int exiting = 0;
    if (exiting++) { /* let's imagine this is atomic... :-) */
        --exiting;
    } else {
        close(sock);
        if (sig) {
            log_printf(0, ZONE, "%s: got signal %d (%s), exiting.", me, sig,
                       SIGNAL_NAME(sig));
        } else {
            log_printf(0, ZONE, "%s: pipe closed (EOF), exiting.", me);
        }
        exit(0);
    }
}

void cleanup_atexit(void) {
    cleanup(0);
    DIE_ERROR(0, ZONE,
              "%s: Stats: %lu entries sent, %lu discarded, %lu failed xmit", me,
              packets, discarded, failed);
}

int main(int argc, char** argv) {
    struct sockaddr_in logserv;

    struct hostent *info;

    int port;
    int i;
    int length, option_index;

    snprintf(me, 32, "logger[%d]", getpid());
#ifdef USE_SYSLOG
    init_syslog( "", 0 ); /* we do everything ourselves */
#endif
    /*
     * Initialize to defaults
     */
    port = LOGGER_PORT;
    strcpy(host, LOGGER_HOST);
    debug = DEBUG_DEFAULT;

    /*
     * Process command line options
     */
    while (1) {
        int c = getopt_long(argc, argv, shorts, longs, &option_index);

        if (c == -1)
            break;

        LOG_PRINTF(DEBUG_MAX, ZONE, "%s: processing option \"-%c %s\"",
                   me, (char) c, optarg);

        switch (c) {

        case 'h':
            strncpy(host, optarg, HOSTNAME_SIZE);
            break;

        case 'p':
            if (atoi(optarg))
                port = atoi(optarg);

        case 'd':
            debug = atoi(optarg);
            break;
        }
    }

    if (optind < argc) { /* there's some argv leftovers */
    };

    if (!(info = gethostbyname(host))) {
        DIE_ERROR(1, ZONE, "%s: gethostbyname(%s): %s", me, host, LAST_ERROR);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        DIE_ERROR(1, ZONE, "%s: socket(SOCK_DGRAM): %s", me, LAST_ERROR);
    }

    bzero((char*) &logserv, sizeof(logserv));
    logserv.sin_family = AF_INET;
    logserv.sin_port = htons(port);
    logserv.sin_addr = *((struct in_addr*) info->h_addr);

    LOG_PRINTF(DEBUG_MIN, ZONE, "%s: Using server %s [%s] on port %d.", me,
               info->h_name, inet_ntoa(logserv.sin_addr), ntohs(logserv.sin_port));
    /* LOG_PRINTF( DEBUG_MED, ZONE, "%s: Reading from stdin...", me ); */

    /*
     * Try to have the cleanup() run if any signal is received that
     * would otherwise kill the program. This whole signal thing is so
     * that we get to print the signal that killed us.
     */
    atexit(cleanup_atexit);
    for (i = 1; i <= 31; i++) {
        signal(i, cleanup);
    }

    while (fgets(buffer, MSG_SIZE, stdin)) {
        length = strlen(buffer);
        if (length >= MSG_SIZE - 1 && buffer[length] != '\n') {
            /*
             * The text sent is longer than our buffer, what to do ?
             * For now, eat the rest of the line.
             */
            while (strlen(buffer) >= MSG_SIZE - 1
                   && buffer[MSG_SIZE - 1] != '\n'
                   && fgets(buffer, MSG_SIZE, stdin)) {
                length += strlen(buffer);
            }
            LOG_PRINTF(DEBUG_MED, ZONE,
                       "%s: Discarded %d bytes (exceeded input buffer)", me,
                       length);
            length = 0;
            ++discarded;
        }
        if (length) {
            buffer[length - 1] = '\0'; /* remove the final '\n' */
            if (sendto(sock, buffer, length, 0, (struct sockaddr*) &logserv,
                       sizeof(logserv)) < 0) {
                LOG_PRINTF(DEBUG_ERROR, ZONE, "%s: sendto( %s:%d ): %s", me,
                           info->h_name, ntohs(logserv.sin_port), LAST_ERROR);
                ++failed;
            } else {
                ++packets;
            }
        }
    }
    return 0; /* keep gcc happy */
}
