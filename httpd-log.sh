#!/bin/sh
#
# $Id$
#
# httpd-log  This shell script takes care of starting and stopping httpd-log.
#
# chkconfig: 2345 80 20
# description: centralized httpd log server
#

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/network

# Check that networking is up.
[ ${NETWORKING} = "no" ] && exit 0

[ -f /usr/sbin/httpd-logd ] || exit 0

HTTP_LOG_ARGS="--daemon"
HTTP_LOG_USER="nobody"
if [ -f /etc/sysconfig/httpd-log ]; then
  . /etc/sysconfig/httpd-log
fi

# See how we were called.
case "$1" in
  start)
        # Start daemons.
        echo -n "Starting httpd-log: "
        daemon /bin/su -s /bin/sh $HTTP_LOG_USER -c "'/usr/sbin/httpd-logd $HTTP_LOG_ARGS'"
        touch /var/lock/subsys/httpd-log
        echo
        ;;
  stop)
        # Stop daemons.
        echo -n "Shutting down httpd-log: "
        killproc httpd-logd
        echo "done"
        rm -f /var/lock/subsys/httpd-log
        ;;
  *)
        echo "Usage: `basename $0` {start|stop}"
        exit 1
esac

exit 0
