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
 * Module for processing a single log request.
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
 * TODO:
 * separate write_log into a different file.
 * use old_log_file to write remaining logs after log_file has changed.
 */

static const char *VERSION = "$Id$";

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logger.h"
#include "fd_cache.h"
#include "debug.h"

char msg_raw[MSG_SIZE + PATH_SIZE + 2 * sizeof(unsigned)];
/*
 * Read the description of the msg format in write_log_process()
 */
char *path_buf = (msg_raw + sizeof(unsigned));
char *msg_buf = (msg_raw + PATH_SIZE + 2 * sizeof(unsigned));

extern int day;     /* both defined in logserver.c */
char log_file[32];
/* 
 * the above will hold the monthly log name i.e. feb.log 
 * Has to be initialized before use. What a useful note.
 */

extern int debug;
extern int detach;
extern int write_log[2];
extern char *logger_spool;

/* 
 * End of future debug.c
 */

/*
 * Format time according to TIMESTAMP_FORMAT
 * second argument is either NULL to use timestamp global var, 
 * or otherwise points to the buffer where the stamp will be written.
 */
char timestamp[TIMESTAMP_SIZE+1];
inline void mk_timestamp( time_t t, char *where ){
  int len, offset;
  if (!where){
    where = timestamp;
  }
  strftime( where, TIMESTAMP_SIZE, TIMESTAMP_FORMAT, localtime(&t) );
#ifdef APACHE_TZ
  offset = timezone / 60; /* that is a global variable */
  len = strlen(where);
  snprintf( where+len, TIMESTAMP_SIZE-len,
            "%c%.2d%.2d]", (offset>0) ? '-' : '+',
            offset/60, offset % 60 );
#endif
}

/*
 * Hash is in the form a/b/abac/ (or a/b/www.abac.com/)
 * The hashed array is pre-allocated.
 * Current directory is NOT changed.
 * NOTE: should increase to 3 elements.
 */
void get_hash( char hashed[PATH_SIZE], char *name ){
  int length = strlen( name );
  char a, b;
  
  if (length < 2){
    memcpy( hashed, "default", sizeof("default") );
  }

  if (length > 10  /* at least www.ab.com */
      && !memcmp( name, "www.", 4 )){
    a = name[4];
    b = name[5];
  } else {
    a = *name;
    b = name[1];
  }
  *hashed++ = a;
  *hashed++ = '/';
  *hashed++ = b;
  *hashed++ = '/';
  memcpy( hashed, name, length );
  hashed[length] = '/';
  hashed[length+1] = '\0';
}

/*
 * Take the first path element from src and put it in firstdir
 * input: src = "/aaa/bbb/ccc"
 * output: dst = "aaa"
 */
void get_first_dir( char *dst, char *src ){
  char *tmp = dst;
  if (*src++ != '/'){
    *dst = '\0';
    return;
  }
  /* skip all leading /, ./ */
  while ( *src == '/' || ( *src == '.' && src[1] == '/' ) ){ src++; };
  while ( *src != '/' && (*tmp++ = *src++) );
  if (*src == '/'){
    *tmp = '\0'; /* we have a pathname in firstdir */
  } else {
    /* no slash found in uri, maybe it's a file name. assume root dir. */
    *dst = '\0';
  }
}

/*
 * Put username in user, change src to the rest of the path
 * Returns 0 if a user pathname could not be found, true otherwise
 */
int get_username( char *user, char **src ){
  char *tmp;
  int length;
  get_first_dir( user, *src );
  if (*user){
    *src += strlen( user ) + 1;
  };
  return *user;
}

/*
 * write to pipe (to write_log presumably)
 */
inline void my_pipe_write( int fd, const void *buf, int size ){
  int wrote;
  static int counter = 0;
  counter++;
  if ((wrote = write( fd, buf, size )) < 0)
    DIE_ERROR( 1, ZONE, "write( pipe, \"%s\" ): %s", (char*)buf, LAST_ERROR );
}

/*
 * Process one log entry
 */
void process_entry( log_entry *rec ){
  static char firstdir[PATH_SIZE+1];
  static char username[PATH_SIZE+1];
  int size, length;
  char *tmp, *log;

  /*
   * See if this is a true VHOST or is using the reseller's site
   */
  if (!rec->root){
    /* 
     * this is a virtual host 
     */
    memcpy( path_buf, PATH_LOG_VHOST, sizeof(PATH_LOG_VHOST) );
    get_hash( path_buf + sizeof(PATH_LOG_VHOST)-1, rec->vhost );
    get_first_dir( firstdir, rec->uri );
  } else {
    /* 
     * this is the main website 
     */
    if (get_username( username, &rec->uri )){
      /* 
       * username could be identified
       * path is of the form PATH_LOG_USER/rec->root/u/s/username
       */
      memcpy( path_buf, PATH_LOG_USER, sizeof(PATH_LOG_USER) );
      length=strlen(rec->root);
      memcpy( path_buf + sizeof(PATH_LOG_USER)-1, rec->root, length );
      length += sizeof(PATH_LOG_USER);
      path_buf[length-1] = '/';
      get_hash( path_buf + length, username );
      get_first_dir( firstdir, rec->uri );
    } else {
      /* 
       * No username, maybe it's the root site pages
       */
      memcpy( path_buf, PATH_LOG_ROOT, sizeof(PATH_LOG_ROOT) );
      length=strlen(rec->root);
      memcpy( path_buf + sizeof(PATH_LOG_ROOT)-1, rec->root, length );
      length += sizeof(PATH_LOG_ROOT);
      path_buf[length-1] = '/';
      path_buf[length] = '\0';
      get_first_dir( firstdir, rec->uri );
    }
  }

  /*
   * At this point we have the log entry record in rec,
   * the path of the log file (not including the file itself) in path_buf
   * and the first directory in firstdir
   */
  if (rec->status >= 200){
    /*
     * Begin prepare raw message. This looks very ugly.
     */
    length = strlen( path_buf );
    tmp = path_buf + length;
    log = log_file;
    for( ; (*tmp = *log); tmp++, log++, length++ ); /* strcat */
    *(unsigned*)msg_raw = length;
    msg_buf = path_buf + length + sizeof(unsigned);
    mk_timestamp( rec->time, NULL );
    snprintf( msg_buf, MSG_SIZE,
#ifdef LOG_EXTENDED
              "%s - %s %s \"%s %s %s\" %d %d \"%s\" \"%s\"",
#else
              "%s - %s %s \"%s %s %s\" %d %d",
#endif
              rec->hostip,
              rec->user,
              timestamp,
              rec->method,
              rec->uri,
              rec->proto,
              rec->status,
              rec->bytes
#ifdef LOG_EXTENDED
              , rec->referrer, rec->useragent
#endif
              );
    length += sizeof(unsigned) * 2 + 
      (*(unsigned*)(msg_buf - sizeof(unsigned)) = strlen( msg_buf ));

    my_pipe_write( write_log[1], msg_raw, length );

    if (!detach){
      LOG_PRINTF( DEBUG_MED, ZONE, "calling write_log_process()..." );
      write_log_process(write_log);
    }
  } else {
    LOG_PRINTF( DEBUG_MAX, ZONE, "discarded %s %s (status %d)", 
                rec->method, rec->uri, rec->status );
  }
}

/*
 * Stuff for write_log process only from here on.
 */

/*
 * Read from the pipe.
 * It retries the read until it has all the data
 */
inline void my_pipe_read( int fd, void *buf, int size ){
  int left = size;
  int recvd = 0;
  while (left){
    if ( (recvd = read( fd, buf+size-left, left )) < 0){
      DIE_ERROR( 1, ZONE, "read( pipe, %d ): %s", size, LAST_ERROR );
    } else {
      left -= recvd;
    }
    if (left)
      LOG_PRINTF( DEBUG_MIN, ZONE, "read( pipe, %d ): %d left to read.", size, left );
  }
}

/*
 * Main loop for write_log process
 *
 * Sits here reading from pipe and writing to log files.
 * What a life...
 * 
 * if nodaemon mode is set, then the loop is only executed once to make sure
 * we do not get stuck in pipe read forever
 */
void write_log_process( int p[2] ){
  int size, fd;
  static int flag = 1;
  int loop_control = 1;
  static int rootdir = 0;

  if (flag){
    init_fd_table();
    flag = 0;
  }

  /*
   * I would expect chdir to a file descriptor to be much faster than
   * chdir to path (because it doesn't have to recheck the path)
   */
   
  if (!rootdir){
    rootdir = open( logger_spool, 0 );
    if (rootdir < 0)
      DIE_ERROR( 5, ZONE, "write_log: open(dir %s): %s", 
                 logger_spool, LAST_ERROR );
  }
  if (fchdir( rootdir )){
    DIE_ERROR( 5, ZONE, "write_log: fchdir(%d) (%s): %s", rootdir, 
               logger_spool, LAST_ERROR );
  }
  
  /*
   * (Internal) Format of stuff sent through pipe is:
   * sizeof(int)    length of next string
   * (above) bytes  filename
   * sizeof(int)    length of next string
   * (above) bytes  log text (w/o newline)
   */
  while( loop_control ){

    my_pipe_read( p[0], &size, sizeof(size) );
    my_pipe_read( p[0], path_buf, size );
    path_buf[size] = '\0';

    my_pipe_read( p[0], &size, sizeof(size) );
    my_pipe_read( p[0], msg_buf, size );
    msg_buf[size] = '\n';

    if (size == sizeof(time_t)){
      /*
       * This was a short message telling to clean up descriptors because
       * log file name has changed.
       */
      close_fd_all( 1, *(time_t *)msg_buf );
    } else {

      fd = get_fd( path_buf );
      if (fd){
        write( fd, msg_buf, size+1 );
        LOG_PRINTF( DEBUG_MAX, ZONE, "wrote %d bytes to %s", size+1, path_buf );
      } else {
        log_printf( 0, ZONE, "write_log: get_fd( %s ): %s, ignored.",
                    path_buf, LAST_ERROR );
        /*
         * The log line is ignored in this case...
         */
      }
    }

    if (!detach){
      loop_control = 0; /* or just break would be enuff. */
    }
  }
}

