#-----------------------------------------------------------------------------
# Copyright (C)2003 InfoStreet, Inc.        www.infostreet.com
# Copyright (C)2007 Laurentiu C Badea (LC)  sourceforge.net/users/wotevah
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
URL: http://www.wotevah.com/
Vendor: LC
License: GPL
Group: System Environment/Daemons
Source: httpd-log.tar.gz
BuildPreReq: httpd-devel

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

%define _prefix /opt/httpd-log

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
mkdir ${RPM_BUILD_ROOT}%{_prefix}/data
perl -p -i -e 's(/opt/httpd-log)(%{_prefix})' logctl
install -D -m 0755 logctl ${RPM_BUILD_ROOT}/etc/init.d/httpd-log
mkdir -p ${RPM_BUILD_ROOT}/etc/httpd/conf.d
cat <<EOF > ${RPM_BUILD_ROOT}/etc/httpd/conf.d/httpd-log.conf
# Do not change the log format - \t is needed for httpd-log-server
LogFormat "%a\t%l\t%u\t%s\t%b\t%v\t%r\t%{Referer}i\t%{User-agent}i\t\t" httplog
CustomLog "|%{_prefix}/bin/logger -p 8181" httplog
EOF

%clean
rm -rf $RPM_BUILD_ROOT

%files server
%defattr(-,root,root)
%{_prefix}/bin/httpdlogd
%config /etc/init.d/httpd-log
%attr(-,nobody,nobody) %{_prefix}/data

%files client
%defattr(-,root,root)
%config(noreplace) /etc/httpd/conf.d/httpd-log.conf
%{_prefix}/bin/logger

%postun server
chkconfig --add httpd-log

