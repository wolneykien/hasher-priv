# $Id$

Name: pkg-build-priv
Version: 0.1.1
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
%_sbindir/pkg-build-useradd
# config
%attr(400,root,root) %config(noreplace) %_sysconfdir/sudo.d/%name
%attr(700,root,root) %dir %configdir
%attr(700,root,root) %dir %configdir/user.d
%attr(600,root,root) %config(noreplace) %configdir/system
# helpers
%attr(750,root,pkg-build) %dir %helperdir
%attr(700,root,root) %helperdir/pkg-build-priv
%attr(755,root,root) %helperdir/*.sh

%doc DESIGN

%changelog
* Wed Apr 09 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1.1-alt1
- chdiruid.c: check for group-writable directory without sticky bit.

* Sun Apr 06 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1-alt1
- Added %_sbindir/pkg-build-useradd.
- Added DESIGN file.

* Sun Apr 06 2003 Dmitry V. Levin <ldv@altlinux.org> 0.0.5-alt1
- Added CALLER_NUM support.

* Fri Apr 04 2003 Dmitry V. Levin <ldv@altlinux.org> 0.0.4-alt1
- priv.h:
  + lowered minimal uid/gid from 100 to 34.
- chrootuid.c:
  + fixed typo.

* Thu Apr 03 2003 Dmitry V. Levin <ldv@altlinux.org> 0.0.3-alt1
- chrootuid.c: set umask (022) unconditionally before exec.

* Mon Mar 31 2003 Dmitry V. Levin <ldv@altlinux.org> 0.0.2-alt1
- priv.h:
  + lowered minimal uid/gid from 500 to 100.
- chdiruid.c:
  + added check for "st_gid != change_gid1";
  + removed check for "st_mode & S_IWGRP".

* Sun Mar 30 2003 Dmitry V. Levin <ldv@altlinux.org> 0.0.1-alt1
- Initial revision.
