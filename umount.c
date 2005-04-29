
/*
  $Id$
  Copyright (C) 2004,2005  Dmitry V. Levin <ldv@altlinux.org>

  The umount action for the hasher-priv program.

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

/* Code in this file may be executed with root privileges. */

#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>

#include "priv.h"
#include "xmalloc.h"

#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif

static int
xumount (const char *chdir_path1, const char *chdir_path2, const char *dir,
	 const char *fsname)
{
	int     unmounted = 0;

	for (;;)
	{
		chdiruid (chdir_path1);
		if (chdir_path2)
			chdiruid (chdir_path2);
		safe_chdir (dir, stat_permok_validator);
		if (umount2 (".", MNT_DETACH) < 0)
			break;
		unmounted = 1;
	}

	if (unmounted)
		errno = 0;
	else if (errno != EINVAL)
		error (EXIT_SUCCESS, errno, "umount: %s", fsname);

	return unmounted;
}

static int
umount_proc (void)
{
	return xumount (chroot_path, 0, "proc", "proc");
}

static int
umount_devpts (void)
{
	return xumount (chroot_path, "dev", "pts", "devpts");
}

static int
umount_sysfs (void)
{
	return xumount (chroot_path, 0, "sys", "sysfs");
}

int
do_umount (void)
{
	char   *targets =
		allowed_mountpoints ? xstrdup (allowed_mountpoints) : 0;
	char   *target;
	int     unmounted = 0;

	if (!targets)
		error (EXIT_FAILURE, 0, "umount: no mount points allowed");

	for (target = targets ? strtok (targets, " \t,") : 0; target;
	     target = strtok (0, " \t,"))
	{
		if (!strcmp (target, "/proc"))
			unmounted |= umount_proc ();
		else if (!strcmp (target, "/dev/pts"))
			unmounted |= umount_devpts ();
		else if (!strcmp (target, "/sys"))
			unmounted |= umount_sysfs ();
		else
			error (EXIT_SUCCESS, 0,
			       "umount: %s: mount point not supported",
			       target);
	}

	free (targets);

	if (!unmounted)
		error (EXIT_FAILURE, 0, "umount: no file systems mounted");

	return 0;
}
