
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid actions for the pkg-build-priv program.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "priv.h"

static int
chrootuid (const char *name, uid_t uid, gid_t gid, const char *epath)
{
	const char *const env[] =
		{ "HOME=/", epath, "SHELL=/bin/sh", "TERM=dumb", 0 };

	if (uid < MIN_CHANGE_UID || uid == getuid ())
		error (EXIT_FAILURE, 0, "chrootuid: invalid uid: %u", uid);

	chdiruid (chroot_path, CHDIRUID_ABSOLUTE);

	endpwent ();
	endgrent ();

	/* Check and sanitize file descriptors again. */
	sanitize_fds ();

	if (chroot (".") < 0)
		error (EXIT_FAILURE, errno, "chroot: %s", chroot_path);

	if (setgroups (0, 0) < 0)
		error (EXIT_FAILURE, errno, "chrootuid: setgroups");

	if (setgid (gid) < 0)
		error (EXIT_FAILURE, errno, "chrootuid: setgid");

	if (setuid (gid) < 0)
		error (EXIT_FAILURE, errno, "chrootuid: setuid");

	execve (chroot_argv[0], (char *const *) chroot_argv,
		(char *const *) env);
	error (EXIT_FAILURE, errno, "chrootuid: execve: %s", chroot_argv[0]);
	return EXIT_FAILURE;
}

int
do_chrootuid1 (void)
{
	return chrootuid (change_user1, change_uid1, change_gid1,
			  "PATH=/sbin:/usr/sbin:/bin:/usr/bin");
}

int
do_chrootuid2 (void)
{
	return chrootuid (change_user2, change_uid2, change_gid2,
			  "PATH=/bin:/usr/bin");
}
