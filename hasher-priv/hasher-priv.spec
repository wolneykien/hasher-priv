Name: hasher-priv
Version: 1.2.10
Release: alt1

Summary: A privileged helper for the hasher project
License: GPL
Group: Development/Other
Packager: Dmitry V. Levin <ldv@altlinux.org>

Source: %name-%version.tar

%define _libexecdir %_prefix/libexec
%define helperdir %_libexecdir/%name
%define configdir %_sysconfdir/%name

Provides: %helperdir
PreReq: coreutils, shadow-utils, glibc-utils
Obsoletes: pkg-build-priv

# Due to libexec hell.
Conflicts: hasher < 0:1.0.9-alt0.M24.1

BuildPreReq: help2man, sisyphus_check >= 0:0.7.11

%description
This package provides helpers for executing privileged operations
required by hasher utilities.

%prep
%setup -q

%build
%make_build CFLAGS="%optflags" libexecdir=%_libexecdir

%install
%makeinstall

%pre
if getent group pkg-build >/dev/null; then
	groupmod -n hashman pkg-build
fi
if [ -d %_sysconfdir/pkg-build-priv -a ! -d %configdir ]; then
	%__mv %_sysconfdir/pkg-build-priv %configdir
fi
/usr/sbin/groupadd -r -f hashman

%files
%_sbindir/hasher-useradd
%_mandir/man?/*
# config
%attr(750,root,hashman) %dir %configdir
%attr(750,root,hashman) %dir %configdir/user.d
%attr(640,root,hashman) %config(noreplace) %configdir/fstab
%attr(640,root,hashman) %config(noreplace) %configdir/system
# helpers
%attr(750,root,hashman) %dir %helperdir
%attr(6710,root,hashman) %helperdir/%name
%attr(755,root,root) %helperdir/*.sh

%doc DESIGN

%changelog
* Mon May 14 2007 Dmitry V. Levin <ldv@altlinux.org> 1.2.10-alt1
- Fixed hasher-priv.conf man section number (#11613).
- Changed "prefix" option meaning from allowed prefix to
  colon-separated list of allowed prefixes.
- Changed system.conf prefix value from "~" to "~:/tmp/.private".
- Made %configdir directory tree not only traversable but also
  readable by "hashman" group members.

* Mon Apr 09 2007 Dmitry V. Levin <ldv@altlinux.org> 1.2.9-alt1
- hasher-useradd: When creating satellite users for a system user,
  make them system users, too (#11416).

* Fri Feb 23 2007 Dmitry V. Levin <ldv@altlinux.org> 1.2.8-alt1
- Changed default nice change value from 10 to 8.
- Added support for new RLIMIT_* types:
  sigpending, msgqueue, nice, rtprio.

* Mon Dec 18 2006 Dmitry V. Levin <ldv@altlinux.org> 1.2.7-alt1
- makedev: Create /dev/full device file.
- makedev: Switch fs gid to 0 during device file creation.
- hasher-useradd: Use gpasswd for better group names handling (#10305).

* Wed Oct 18 2006 Dmitry V. Levin <ldv@altlinux.org> 1.2.6-alt1
- Allowed "user.d" configs to override wlimits defined in "system" config.

* Sun Oct 15 2006 Dmitry V. Levin <ldv@altlinux.org> 1.2.5-alt1
- Fixed build with -D_FORTIFY_SOURCE=2 -Werror.

* Sat Mar 18 2006 Dmitry V. Levin <ldv@altlinux.org> 1.2.4-alt1
- makeconsole: New mode, creates console-specific root-only
  devices initially introduced by 1.2.2's makedev.
- makedev.sh: In addition to makedev, call makeconsole
  if enabled by $makedev_console.

* Sat Jan 21 2006 Dmitry V. Levin <ldv@altlinux.org> 1.2.3-alt1
- Makefile: Corrected LFS_CFLAGS.
- child.c: Reworked xauth_add_entry() to support various xauth locations.

* Sun Oct 09 2005 Dmitry V. Levin <ldv@altlinux.org> 1.2.2-alt1
- If use_pty is not set, handle child's stdout and stderr separately.
- In makedev mode, create few devices available to root only (mouse@).

* Mon Aug 15 2005 Dmitry V. Levin <ldv@altlinux.org> 1.2.1-alt1
- hasher-priv: Do not lowercase mount points (at@).
- chrootuid1.sh: Synced with chrootuid2.sh.
- DESIGN: Fixed typo (at@).

* Sat Jul 16 2005 Dmitry V. Levin <ldv@altlinux.org> 1.2.0-alt1
- Implemented X11 authentication spoofing.
- Implemented custom mounts support via %configdir/fstab.

* Sat Jul 09 2005 Dmitry V. Levin <ldv@altlinux.org> 1.1.0-alt1
- Implemented X11 forwarding.

* Sat Apr 30 2005 Dmitry V. Levin <ldv@altlinux.org> 1.0.5-alt1
- Fixed umount looping on 2.6 kernel (closes #6667).

* Sun Mar 13 2005 Dmitry V. Levin <ldv@altlinux.org> 1.0.4-alt1
- When making device files inside chroot,
  first try to hardlink existing device files,
  second try to create them using mknod(2).
  This approach simplifies usage in restricted environments
  where mknod(2) is not allowed even for superuser.

* Mon Jan 03 2005 Dmitry V. Levin <ldv@altlinux.org> 1.0.3-alt1
- Changed helper directory to %helperdir.
- Updated documentation:
  + hasher-priv.conf(5): s/lim_/limit_/ (fixes #5805);
  + hasher-priv(8): fix NAME section, document TERM variable;
  + hasher-useradd(8): fix NAME section.

* Thu Nov 18 2004 Dmitry V. Levin <ldv@altlinux.org> 1.0.2-alt1
- Changed privileged helper to suid program,
  to get rid of sudo dependence.

* Sat Sep 11 2004 Dmitry V. Levin <ldv@altlinux.org> 1.0.1-alt1
- Enhanced use_pty mode:
  pass $TERM value, translate window size changes.
- Pass libexecdir to %%make_build (#4902).

* Thu Jul 15 2004 Dmitry V. Levin <ldv@altlinux.org> 1.0-alt1
- Added hasher-priv.conf(5) manpage.
- Added more docs to hasher-priv(8) manpage.

* Tue Jul 13 2004 Dmitry V. Levin <ldv@altlinux.org> 0.9.9-alt1
- maketty: new mode, controlled by allow_ttydev config option.
- chrootuid: use pty for communicating with child,
  controlled by use_pty environment option.

* Fri Jul 09 2004 Dmitry V. Levin <ldv@altlinux.org> 0.9-alt1
- Implemented mount/umount modes, controlled by
  allowed_mountpoints config option.
- New config option: allowed_mountpoints.
- DESIGN: document it.

* Wed Jul 07 2004 Dmitry V. Levin <ldv@altlinux.org> 0.8-alt1
- config:
  + read work limit hints from environment variables;
  + use lstat+chdir+lstat instead of open+fstat+fchdir+close.

* Tue Jul 06 2004 Dmitry V. Levin <ldv@altlinux.org> 0.7.1-alt1
- chroot prefix: trim trailing slashes.

* Fri Jan 02 2004 Dmitry V. Levin <ldv@altlinux.org> 0.7-alt1
- Deal with compilation warnings generated by new gcc compiler.
- Build with -W -Wall -Werror by default.
- Enhanced prefix mismatch diagnostics.

* Wed Oct 15 2003 Dmitry V. Levin <ldv@altlinux.org> 0.6.1-alt1
- Fixed exit code translation error introduced in previous release.

* Tue Oct 14 2003 Dmitry V. Levin <ldv@altlinux.org> 0.6-alt1
- config, chrootuid{1,2}: handle work limits.

* Sun Sep 21 2003 Dmitry V. Levin <ldv@altlinux.org> 0.5-alt1
- chrootuid{1,2}: call killuid on signal arrival.

* Sun Sep 07 2003 Dmitry V. Levin <ldv@altlinux.org> 0.4-alt1
- killuid: purge all SYSV IPC objects.

* Wed Jul 02 2003 Dmitry V. Levin <ldv@altlinux.org> 0.3-alt1
- Renamed project to hasher-priv.
- Renamed pkg-build group to hashman.

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