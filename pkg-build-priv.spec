# $Id$

Name: pkg-build-priv
Version: 0.0.1
Release: alt1

Summary: A privileged helper for the pkg-build project
License: GPL
Group: Development/Other

Source: %name-%version.tar.bz2

PreReq: shadow-utils, sudo

%define helperdir %_libexecdir/pkg-build-priv
%define configdir %_sysconfdir/pkg-build-priv

%description
This package provides helpers for executing privileged operations
required by pkg-build utilities.

%prep
%setup -q

%build
%make_build

%install
%makeinstall
%__install -pD -m400 %name.sudoers $RPM_BUILD_ROOT%_sysconfdir/sudo.d/%name

%pre
/usr/sbin/groupadd -r -f pkg-build

%files
# config
%attr(400,root,root) %config(noreplace) %_sysconfdir/sudo.d/%name
%attr(700,root,root) %dir %configdir
%attr(700,root,root) %dir %configdir/user.d
%attr(600,root,root) %config(noreplace) %configdir/system
# helpers
%attr(750,root,pkg-build) %dir %helperdir
%attr(700,root,root) %helperdir/pkg-build-priv
%attr(755,root,root) %helperdir/*.sh

%changelog
* Sun Mar 30 2003 Dmitry V. Levin <ldv@altlinux.org> 0.0.1-alt1
- Initial revision.
