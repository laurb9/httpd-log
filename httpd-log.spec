#-----------------------------------------------------------------------------
# Copyright (C)2003 InfoStreet, Inc.        www.infostreet.com
# Copyright (C)2007,2009 Laurentiu Badea    sourceforge.net/users/wotevah
#
# Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
# Created:  2003
# $LastChangedDate$
# $LastChangedBy$
# $Revision$
#
# Description:
# RPM Spec File
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

Summary: Centralized HTTP logging server
Name: httpd-log
Version: 1.0
Release: 2
URL: http://httpd-log.sourceforge.net/
License: GPL
Group: System Environment/Daemons
Source: httpd-log.tar.gz

BuildRoot: %{_tmppath}/%{name}-root

%description
Centralized HTTP logging server.
Creates per-virtual host daily log files for further offline processing.

%package server
Summary: Centralized HTTP logging server
Group: System Environment/Daemons

%description server
Centralized httpd logging server.
Receives and manages access log data from multiple HTTP servers that are
using httpd-log-client. Creates daily log files in per-virtual-host
directories.

%package client
Summary: HTTP logging client
Group: System Environment/Daemons
Requires: httpd

%description client
httpd access logging client - on the httpd side.
Runs on pipe under httpd, sends log entries to an httpd-log-server
running either locally or on another machine.

%prep
%setup -q -n %{name}
aclocal
autoheader
automake --foreign --add-missing
autoconf

%configure

%build
%{__make}

%install
test -z "$RPM_BUILD_ROOT" -o "$RPM_BUILD_ROOT" == "/" && exit 1
%{__rm} -rf $RPM_BUILD_ROOT
%{__make} DESTDIR=$RPM_BUILD_ROOT install
mkdir -p ${RPM_BUILD_ROOT}/var/log/httpd-log/{vhosts,users,root}
mkdir -p ${RPM_BUILD_ROOT}/etc/httpd/conf.d
mkdir -p ${RPM_BUILD_ROOT}/etc/sysconfig
install -D -m 0755 httpd-log.sh ${RPM_BUILD_ROOT}/etc/init.d/httpd-log
install -D -m 0644 httpd-log.conf ${RPM_BUILD_ROOT}/etc/httpd/conf.d/httpd-log.conf
cat <<EOF > ${RPM_BUILD_ROOT}/etc/sysconfig/httpd-log
HTTP_LOG_ARGS="--debug 1 --listen localhost --daemon --spool /var/log/httpd-log"
HTTP_LOG_USER="nobody"
EOF

%clean
rm -rf $RPM_BUILD_ROOT

%files server
%defattr(-,root,root)
/usr/sbin/httpd-logd
%config /etc/init.d/httpd-log
%config(noreplace) /etc/sysconfig/httpd-log
%dir /var/log/httpd-log
%attr(-,nobody,nobody) /var/log/httpd-log/*

%files client
%defattr(-,root,root)
%config(noreplace) /etc/httpd/conf.d/httpd-log.conf
/usr/bin/httpd-logger

%post server
/sbin/chkconfig --add httpd-log

%preun server
if [ $1 = 0 ] && /sbin/chkconfig httpd-log 2>/dev/null; then
    /sbin/service httpd-log stop
    echo "Removing httpd-log service"
    chkconfig --remove httpd-log
fi
