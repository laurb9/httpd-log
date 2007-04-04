/*
 * Copyright (C)1999-2002 InfoStreet, Inc.    www.infostreet.com
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Mar 26, 1999
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * Generic logging/debugging support via stderr or syslog
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

#include "debug.h"

int gDebug = DEBUG_ERROR; /* can also be DEBUG_MIN, DEBUG_MED, DEBUG_MAX */

#ifdef USE_SYSLOG

int stderr_too = 1;  /* set to print errors to stderr by default if input is a terminal */

/* this size is easy to overflow if we dump the received messages */
#define SYSLOG_SIZE 256
char syslog_buf[SYSLOG_SIZE];

/*
 * Signal handlers so we can tell what killed us
 */
static void cleanup(int sig){
  static int exiting = 0;
  if (exiting++){ /* let's imagine this is atomic... :-) */
    --exiting;
  } else {
    if (sig){
      log_printf( DEBUG_ERROR, ZONE, "got signal %d (%s), exiting.", sig, 
                  SIGNAL_NAME(sig) );
      exit(-sig);
    };
  }
}

static void cleanup_atexit(void){
  cleanup(0);
}

void init_syslog( char *ident, int options ){
  openlog( ident, LOG_CONS|options, LOG_DAEMON );
}

#endif

void debug_init( char *ident, int debug_level, int options ){
  int i;
#ifdef USE_SYSLOG
  init_syslog( ident, options );
#endif
  gDebug = debug_level;
  atexit(cleanup_atexit);
  for ( i=1; i<=31; i++ )
    signal( i, cleanup );

}

void die_printf( int code, char *file, int line, const char *format, ... ){
  int len;
  va_list ap;
  va_start(ap, format);

#ifdef USE_SYSLOG
  len = snprintf( syslog_buf, SYSLOG_SIZE, "%s:%d ", file, line );
  vsnprintf( syslog_buf, SYSLOG_SIZE-1-len, format, ap );
  syslog( (code) ? LOG_ERR : LOG_WARNING, "%s", syslog_buf );
  if (isatty(0) && stderr_too){
    fprintf( stderr, "%s\n", syslog_buf );
  }
#else

  fprintf( stderr, "%s:%d", file, line );
  vfprintf( stderr, format, ap );
  fprintf( stderr, "\n" );
#endif

  va_end(ap);
  exit(code);
};

void log_printf( int level, char *file, int line, const char *format, ... ){
  int len;
  va_list ap;
  va_start(ap, format);

  if (level > gDebug)
    return;

#ifdef USE_SYSLOG
  len = snprintf( syslog_buf, SYSLOG_SIZE, "%s:%d ", file, line );
  vsnprintf( syslog_buf+len, SYSLOG_SIZE-1-len, format, ap );
  syslog( (gDebug) ? ((gDebug > 1) ? LOG_DEBUG : LOG_INFO) : LOG_ERR,
          "%s", syslog_buf );
  if (isatty(0) && stderr_too) {
    fprintf( stderr, "%s\n", syslog_buf );
  }
#else

  fprintf( stderr, "%s:%d", file, line );
  vfprintf( stderr, format, ap );
  fprintf( stderr, "\n" );
#endif

  va_end(ap);
}


