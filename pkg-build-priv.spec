# $Id$

Name: pkg-build-priv
Version: 0.2.1
Release: alt1

Summary: A privileged helper for the pkg-build project
License: GPL
Group: Development/Other

Source: %name-%version.tar.bz2

PreReq: shadow-utils, sudo

# Automatically added by buildreq on Fri May 02 2003
BuildRequires: help2man

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
%_mandir/man?/*
# config
%attr(400,root,root) %config(noreplace) %_sysconfdir/sudo.d/%name
%attr(710,root,pkg-build) %dir %configdir
%attr(710,root,pkg-build) %dir %configdir/user.d
%attr(640,root,pkg-build) %config(noreplace) %configdir/system
# helpers
%attr(750,root,pkg-build) %dir %helperdir
%attr(2710,root,pkg-build) %helperdir/pkg-build-priv
%attr(755,root,root) %helperdir/*.sh

%doc DESIGN

%changelog
* Thu Jun 26 2003 Dmitry V. Levin <ldv@altlinux.org> 0.2.1-alt1
- pkg-build-priv:
  + fixed typo in usage text;
  + in chrootuid, export user-dependent USER variable.
- pkg-build-useradd: add user also to the main group of user2.

* Sat May 10 2003 Dmitry V. Levin <ldv@altlinux.org> 0.2.0-alt1
- Config file parser now supports options for setting umask,
  nice and resource limits.
- Set umask=022 and nice=10 by default
  (same values which was hardcoded before).
- Make config files readable by users.
- chrootuid{1,2}.sh: do killuid call before chrootuid call
  as well as after chrootuid call.

* Tue May 06 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1.6-alt1
- pkg-build-priv:
  + added --version option;
  + added help2man-generated manpage.

* Mon May 05 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1.5-alt1
- chrootuid.c: set nice to 10.

* Thu May 01 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1.4-alt1
- chrootuid.c: pass user-dependent HOME to spawned process,
  not just "HOME=/" as before.

* Tue Apr 29 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1.3-alt1
- chdiruid.c: extended error diagnostics.

* Sat Apr 12 2003 Dmitry V. Levin <ldv@altlinux.org> 0.1.2-alt1
- killuid.c: fixed build and work on linux kernel 2.2.x
- chrootuid.c: added /usr/X11R6/bin to the PATH of second user
- Install helper setgid pkg-build to ensure dumpable flag is unset.

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
