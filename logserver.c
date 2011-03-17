/*
 * Copyright (C)1999-2003 InfoStreet, Inc.    www.infostreet.com
 * Copyright (C)2009-2011 Laurentiu Badea     sourceforge.net/users/wotevah
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 23, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * Logger daemon. Receives log lines on predefined UDP port and processes them
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

static const char *VERSION __attribute__ ((used)) = "$Id$";

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"
#include "fd_cache.h"
#include "debug.h"

char host[HOSTNAME_SIZE + 1];
char buffer[MSG_SIZE + 1];
int sock; /* file descriptor id for our socket */
int port; /* port where we listen to           */

char *logger_spool = LOGGER_SPOOL;

int timer_expired = 0;
int day = 0;
time_t time_limit = 0;
extern char log_file[]; /* defined in log_entry.c */
extern char *SIGNAL_NAME(int); /* defined in signalnames.c */

int write_log[2]; /* pipe for communication with write_log process */
int write_log_pid = 0;

int debug = DEBUG_DEFAULT;
int detach = DEFAULT_DETACH;

int action = 0; /* this will hold the name of the signal caught */
int packets_received = 0;

struct option longs[] = {
    {"listen",  required_argument, NULL, 'l'},
    {"port",    required_argument, NULL, 'p'},
    {"debug",   required_argument, NULL, 'd'},
    {"nodaemon",      no_argument, NULL, 'n'},
    {"daemon",        no_argument, NULL, 'D'},
    {"spool",   required_argument, NULL, 's'},
    {"unknown", 0, NULL, 0}
};

const char shorts[] = "l:p:d:nDs:";

log_entry log_buffer[LOG_ENTRIES];
int log_counter = 0;

int child_counter = 0;
/*
 * message structure passed to write_log when log file name is changed
 * 2 x ( length, msg )
 */
char write_log_msg[2* (sizeof (unsigned)+sizeof(time_t))];
char *write_log_tmp; /* helper pointer for above */
time_t logfd_age = 0;

void update_log_file(void); /* defined later in this file */
pid_t spawn_write_log(void);

/*
 * Loop that waits for something to come up, and checks for signals
 * in the meanwhile
 */
#define MSG_IN_QUEUE 1
#define SIGNAL_CAUGHT 2
int wait_loop(int sock) {
    static fd_set read_fds;
    static int status, child_pid;
    struct timeval timeout;
    int nfds = 0;

    FD_ZERO(&read_fds);

    while (!nfds) {
        FD_SET(sock, &read_fds);

        /* we only use the timeout to clean up the children */
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;

        /*
         * Clean up the dead children
         */
        if (child_counter && (child_pid = waitpid(0, &status, WNOHANG))) {
            child_counter--;
            if (WIFEXITED(status)) {
                LOG_PRINTF(DEBUG_MED, ZONE,
                           "wait_loop: child %d exited with status %d",
                           child_pid, WEXITSTATUS(status));
            } else {
                LOG_PRINTF(DEBUG_MIN, ZONE, "wait_loop: child %d %s.",
                           child_pid,
                           WIFSIGNALED(status) ? "was killed" : "died prematurely");
            }
            if (child_pid == write_log_pid) { /* oops, __it happens */
                child_counter++; /* it wasn't the one we expected */
                LOG_PRINTF(DEBUG_ERROR, ZONE,
                           "WARNING: write_log (PID %d) died, trying to respawn",
                           write_log_pid);
                spawn_write_log();
            }
        }

        /*
         * Check the log file name (maybe it needs to be changed)
         */
        update_log_file();

        /*
         * See if we changed the log file, and if so, signal write_log to clean
         * up its descriptor table. We only do that after all children have died
         * (since I cannot differentiate the old ones from the new ones)
         */
        if (logfd_age && !child_counter) {
            /*
             * This part would look nicer if structs were guaranteed
             * to preserve the order of their fields.
             */
            write_log_tmp = write_log_msg;
#define WRITE_LOG_ADD(type, value) \
      *(type*)write_log_tmp = value; \
      write_log_tmp += sizeof(type);

            WRITE_LOG_ADD(unsigned, sizeof(time_t));
            WRITE_LOG_ADD(time_t, 0);
            WRITE_LOG_ADD(unsigned, sizeof(time_t));
            WRITE_LOG_ADD(time_t, logfd_age);
#undef WRITE_LOG_ADD

            write(write_log[1], &write_log_msg, write_log_tmp - write_log_msg);
            logfd_age = 0;

            if (!detach) {
                LOG_PRINTF(DEBUG_MED, ZONE,
                           "calling write_log_process() to close fds");
                write_log_process(write_log);
            }
        }

        /*
         * Safety to prevent forking too many children at the same time.
         * We refuse receiving data if we have this many children still alive
         */
        if (child_counter > MAX_LIVE_CHILDREN) {
            LOG_PRINTF(DEBUG_MIN, ZONE,
                       "wait_loop: stopped receiving data, too many children (%d)",
                       child_counter);
            nfds = 0;
        } else {
            nfds = select(FD_SETSIZE, (fd_set *) &read_fds, (fd_set *) NULL,
                          (fd_set *) NULL, &timeout);
        }
        if (nfds < 0) {
            if (errno == EINTR) {
                return SIGNAL_CAUGHT;
            } else {
                LOG_PRINTF(0, ZONE, "main select() failed: %s", LAST_ERROR);
            }
        }
        if (action) {
            return SIGNAL_CAUGHT;
        }
    }
    packets_received++;
    return MSG_IN_QUEUE;
}

/*
 * Generic signal handler
 * We don't restore the handler to prevent a race (?)
 */
void signal_catch(int something) {
    action = something;
    /* signal( something, signal_catch ); */
}

/*
 * Process command-line arguments
 */
void command_line(int argc, char **argv) {
    int option_index;
    /*
     * Initialize to defaults
     */
    port = LOGGER_PORT;
    if (gethostname(host, HOSTNAME_SIZE)) {
        LOG_PRINTF(0, ZONE, "gethostname(%s): %s", host, LAST_ERROR);
        strcpy(host, "localhost");
    }
    debug = DEBUG_DEFAULT;

    /*
     * Process command line options
     */
    while (1) {
        int c = getopt_long(argc, argv, shorts, longs, &option_index);

        if (c == -1)
            break;

        switch (c) {

        case 'l':
            strncpy(host, optarg, HOSTNAME_SIZE);
            break;

        case 'p':
            if (atoi(optarg))
                port = atoi(optarg);
            break;

        case 'd':
            debug = atoi(optarg);
            break;

        case 'n':
        case 'D':
            detach = (c == 'D');
            break;

        case 's':
            logger_spool = strdup(optarg);
            break;
        }
    }

    if (optind < argc) { /* there's some argv leftovers */
    };
}

/*
 * Generate log file name for this month. Set time value for next day.
 * This is supposed to be called every so often, or (preferrably) set
 * an alarm to wake up when the new time_limit is reached.
 *
 * This is supposed to be called once for init and then at the parent
 * end of process_batch, but it only gets executed if conditions are
 * met anyways so you can call it more often.
 */
void process_batch(void); /* prototype because we define it later */
void update_log_file(void) {
    static struct tm *current;
    time_t time_left;

    /* day is 0 when it's first time we call */
    if (time(NULL) > time_limit || day == 0) {
        /*
         * we would need to process the log entries that still use the old file
         */
        process_batch();

        time_limit = time(NULL);
        logfd_age = time_limit; /* all fd-s older than this will be closed */
        current = localtime(&time_limit);
        day = current->tm_mday;
        strftime(log_file, 32, LOG_FILE_FORMAT, current);
        /*
         * Calculate number of seconds left to finish the day, add to time_limit.
         * This is gonna become the threshold for when to change the string.
         */

#ifndef DEBUG_UPDATELOGFILE
        time_left = current->tm_hour * 3600 + current->tm_min * 60 + current->tm_sec;
        time_limit += 86400 - time_left;
#else
        time_left = DEBUG_UPDATELOGFILE;
        time_limit += time_left;
#endif

        LOG_PRINTF(DEBUG_MIN, ZONE, "Log file name set to %s, next check %s",
                   log_file, ctime(&time_limit));
    }
}

/*
 * Process one log batch. To be called often (minutes)
 */
void process_batch(void) {
    int i, pid;
    static int delay = 1;

    if (!log_counter) {
        return;
    }

    LOG_PRINTF(2, ZONE, "started processing batch, %d entries", log_counter);
    /*
     * Do something with the stuff we've accumulated
     */
    /*
     * Detach process in background
     */
    if (detach) {
        pid = fork();
        i = 0;
    } else {
        pid = -1;
        i = -1;
        LOG_PRINTF(DEBUG_MIN, ZONE, "fg mode, staying in one piece (no fork)",
                   debug);
    }
    if (pid < 0 && i == 0) {
        log_printf(DEBUG_ERROR, ZONE,
                   "fork(process_batch): \"%s\", running in same process, "
                   "inserting delay=%ds", LAST_ERROR, delay);
        /*
         * This, in case the system is really busy, will give some
         * breathing room before we do our stuff.
         */
        sleep(delay);
        if (delay < MAX_FAILEDFORK_DELAY) {
            delay *= 2; /* for every failure wait longer and longer */
        }
        /*
         * switch to same process mode
         */
        i = -1; /* so we do not ignore all the signals below */
    } else {
        delay = 1; /* reset delay value */
    }
    if (pid <= 0) {
        if (i == 0) {
            /* setsid();  *//* so that parent doesn't have to clean up */
#ifdef USE_SYSLOG
            init_syslog( "process_batch", LOG_PID );
#endif
            write_log_pid = 0; /* make sure we don't kill it (search for atexit) */
            signal(SIGHUP, SIG_IGN);
            signal(SIGUSR1, SIG_IGN);
            signal(SIGUSR2, SIG_IGN);
            signal(SIGALRM, SIG_IGN);
        }
        /*
         * Process the data
         */
        for (i = 0; i < log_counter; i++) {
            process_entry(log_buffer + i);
        }

        if (pid == 0) {
            /* setsid(); */
            exit(0);
        }
    } else {
        /*
         * Record this child
         */
        if (i == 0) {
            LOG_PRINTF(DEBUG_MAX, ZONE, "process_batch: forked child(PID=%d)",
                       pid);
            child_counter++;
        }
    }
    log_counter = 0;
    update_log_file();
}

/*
 * Scan for separator in string, update char pointer and length
 * corresponding to the position of the character found:
 * where gets the address of the next character (after separator)
 * length is updated with the length of the rest of the string
 *
 * If separator cannot be found it returns 0
 * This is some sort of a customized and inlined version of memchr.
 */
inline int find_sep(char **where, int *length, char sep) {
    char *tmp = *where;
    for (; *tmp != sep && *length; (*length)--, tmp++)
        ;
    if (!*length) {
        return 0;
    }
    *tmp = '\0';
    *where = tmp + 1;
    return (*length)--;
}

/*
 * Parse one log entry and populate the log_entry structure
 * in the corresponding table slot
 */
void parse_entry(char *buffer, int length) {
    /*
     * I'll use the mem* family instead of their string counterparts
     * because they might be faster
     */
    log_entry *this_entry;
    char *pos, *tmp, *save;
    char pointer;
    /*
     * Stop alarm clock
     */
    alarm(0);
    /*
     * Process received data
     */
    this_entry = log_buffer + log_counter;
    this_entry->time = time(NULL);
    memcpy(this_entry->logline, buffer, length); /* copy line */
    pos = this_entry->logline;

    /*
     * Parse the source logline into its components.
     * The format understood is defined in httpd-log.conf as:
     * "%a\t%u\t%s\t%b\t%v\t%P\t%T\t%r\t%{Referer}i\t%{User-agent}\t"
     */
    while (pos) { /* this loop will be executed only once though */

        /* IP Address */
        this_entry->hostip = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;

        /* identd remote user (usually '-') */
        this_entry->remote_user = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;

        /* authenticated user, if any */
        this_entry->user = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;

        /* Status of request */
        tmp = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;
        this_entry->status = atoi(tmp);

        /* Bytes transferred */
        tmp = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;
        this_entry->bytes = (*tmp == '-') ? -1 : atoi(tmp);

        /* Virtual host (ServerName) */
        this_entry->vhost = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;

        /* save REQUEST */
        save = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;
        tmp = pos; /* so we have start and end of request */

        /* Referrer */
        this_entry->referrer = pos;
        if (!find_sep(&pos, &length, LOG_FIELD_SEPARATOR))
            break;

        /* User Agent */
        this_entry->user_agent = pos;
        find_sep(&pos, &length, LOG_FIELD_SEPARATOR);

        /*
         * Now go back and parse REQUEST to get to method, uri and protocol
         * Looks like "GET /something HTTP/1.0"
         */
        pos = save;
        length = tmp - pos - 1;
        this_entry->method = pos;
        if (!find_sep(&pos, &length, ' '))
            break;

        this_entry->uri = pos;
        if (!find_sep(&pos, &length, ' '))
            break;

        /* we are not interested in the protocol version right now */
        this_entry->proto = pos;
        pos = NULL;
    }

    if (pos) {
        /*
         * This log's format could not be understood
         * Ignore it. But first we need to restore the parsed portion
         * (since we sprinkled some \0-s around during parsing).
         */
        save = this_entry->logline;
        pointer = *pos;
        *pos = '^';
        for (tmp = pos; tmp >= save; tmp--) {
            if (!*tmp) {
                *tmp = '\'';
            }
        }
        LOG_PRINTF(DEBUG_MIN, ZONE, "ignoring from '%c' in \"%s\" pos %d",
                   pointer, save, pos - save);
    } else {

        if (++log_counter == LOG_ENTRIES) {
            /*
             * Process log entries
             */
            process_batch();
        }
    }
    /*
     * Restart alarm clock. The handler must be set somewhere else.
     */
    alarm(LOG_TIMEOUT);
}

/*
 * Function to clean up the child zombie (write_log)
 */
void clean_write_log(void) {
    int status;
    if (write_log_pid) {
        kill(write_log_pid, SIGTERM); /* make sure it dies with us */
        waitpid(write_log_pid, &status, 0);
    }
}

/*
 * Function to spawn write_log helper process.
 * We have it as a function so we can call it again in case it dies.
 */
pid_t spawn_write_log(void) {
    pid_t pid;
    static int counter = 0;
    int complained = 0;

    write_log_pid = 0;
    /*
     * Try to fork child. If it doesn't work then log the error and keep
     * trying (don't give up). But don't complain too often.
     */
    while ((pid = fork()) < 0) {
        if (time(NULL) - complained > 600) {
            LOG_PRINTF(DEBUG_ERROR, ZONE, "fork(write_log): %s", LAST_ERROR);
        }
        complained = time(NULL);
        sleep(10); /* we can use sleep because we know alarm() is not set */
    }

    if (pid == 0) {
        /*
         * write_log helper (child)
         */
#ifdef USE_SYSLOG
        init_syslog( "write_log", LOG_PID );
#endif
        write_log_process(write_log);
        DIE_ERROR(0, ZONE, "write_log process exited.");
        /*
         * end of write_log
         */
    } else {
        write_log_pid = pid;

        /* register to kill it and clean up before we exit */
        if (!counter) {
            atexit(clean_write_log);
        }
        counter++;

        LOG_PRINTF(DEBUG_MIN, ZONE, "write_log process #%d running (PID %d)",
                   counter, pid);
    }
    return pid;
}

int main(int argc, char** argv) {

    struct sockaddr_in logserv, client;
    struct hostent *info;
    socklen_t length;

    int received, retries;
    int store_action;

#ifdef USE_SYSLOG
    init_syslog( "logserver", LOG_PID );
#endif
    command_line(argc, argv);

    /*
     * Should not rely on current directory after calling log_entry though...
     */
    if (chdir(logger_spool)) {
        DIE_ERROR(5, ZONE, "chdir %s: %s", logger_spool, LAST_ERROR);
    } else {
        LOG_PRINTF(DEBUG_MAX, ZONE, "Using spool dir \"%s\"", logger_spool);
    }
    if (!(info = gethostbyaddr(host, strlen(host), 0))) {
        if (!(info = gethostbyname(host))) {
            DIE_ERROR(1, ZONE, "gethostbyname( %s ): %s", host, LAST_ERROR);
        }
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        DIE_ERROR(1, ZONE, "socket(SOCK_DGRAM): %s", LAST_ERROR);
    }

    bzero((char*) &logserv, sizeof(logserv));
    logserv.sin_family = AF_INET;
    logserv.sin_port = htons(port);
    logserv.sin_addr = *((struct in_addr*) info->h_addr);

    retries = 7; /* should be defineable */
    while (retries && bind(sock, (struct sockaddr*) &logserv, sizeof(logserv))
            < 0) {
        retries--;
        if (retries) {
            LOG_PRINTF(DEBUG_ERROR, ZONE,
                       "WARNING: bind(%s:%d): %s, retrying", host, port,
                       LAST_ERROR);
            sleep(3);
        } else {
            DIE_ERROR(1, ZONE, "Cannot bind to %s:%d, exiting", host, port);
        }
    }

    if (retries < 7) {
        LOG_PRINTF(DEBUG_ERROR, ZONE, /* we need to log success on same level */
                   "Listening on %s port %d", host, port);
    } else {
        LOG_PRINTF(DEBUG_MIN, ZONE, "Listening on %s port %d", host, port);
    }

    length = sizeof(client);

    /*
     * Create pipe for communication with the write_log process
     */
    if (pipe(write_log)) {
        DIE_ERROR(3, ZONE, "pipe(): %s", LAST_ERROR);
    }

    /*
     * Fork the daemons
     */
    if (detach) {
        /*
         * Detach main process
         */
        switch ((received = fork())) {

        case -1: /* error */
            DIE_ERROR(2, ZONE, "fork(main): %s", LAST_ERROR);

        case 0: /* child - will outlive parent and will stay in background */
            if (!debug) {
                close(0);
                close(1);
                close(2);
                setsid();
            }
            break;

        default: /* parent */
            DIE_ERROR(0, ZONE, "logserver_main detached with PID %d", received);
        }
        /*
         * Only the child reaches this point; the parent has exited above.
         *
         * Fork the write_log helper process
         */
        spawn_write_log();

    } else {
        LOG_PRINTF(DEBUG_ERROR, ZONE, "Running in foreground, pid %d", getpid());
    }

    signal(SIGINT, signal_catch);
    signal(SIGTERM, signal_catch);
    signal(SIGHUP, signal_catch);
    signal(SIGUSR1, signal_catch);
    signal(SIGUSR2, signal_catch);
    signal(SIGALRM, signal_catch);

    update_log_file(); /* Make sure we have a valid log file name */

    /*
     * Main server loop
     */
    while ((received = wait_loop(sock))) {
        switch (received) {

        case MSG_IN_QUEUE:
            if ((received = recvfrom(sock, buffer, MSG_SIZE, 0,
                    (struct sockaddr*) &client, &length)) >= 0) {
                buffer[received] = '\0';

                LOG_PRINTF(DEBUG_MAX, ZONE, "Received %d bytes from %s",
                           received, inet_ntoa(client.sin_addr));
                LOG_PRINTF(DEBUG_MAX, ZONE, "%s", buffer);

                parse_entry(buffer, received);

            } else {
                /*
                 * This should probably be done using syslog()
                 */
                log_printf(DEBUG_ERROR, ZONE, "recvfrom: %s", LAST_ERROR);
            }
            break;

        case SIGNAL_CAUGHT:
            /*
             * This allows us to exit using signals and stuff
             */
            store_action = action; /* save signal value as it may change */
            switch (store_action) {

            case SIGALRM:
            case SIGHUP:
                action = 0;
                process_batch();
                signal(action, signal_catch); /* restore handler after op. */
                break;

            case SIGTERM:
            case SIGINT:
            default:
                /*
                 * Die gracefully
                 */
                LOG_PRINTF(DEBUG_MIN, ZONE, "Caught signal %d (%s)",
                           store_action, SIGNAL_NAME(store_action));
                process_batch();
                if (write_log_pid) {
                    kill(write_log_pid, SIGTERM);
                }
                LOG_PRINTF(0, ZONE, "Stats: %d packets received.",
                           packets_received);
                DIE_ERROR(0, ZONE, "Exiting on signal %d (%s)", store_action,
                          SIGNAL_NAME(store_action));
            }
        }
    }
    close(sock);
    DIE_ERROR(0, ZONE, "Exiting abnormally. You should never see this message.");
    return 0; /* make GCC happy */
}
