
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
xumount (const char *mpoint)
{
	if (mpoint[0] != '/')
		error (EXIT_FAILURE, EINVAL, "xumount: %s", mpoint);

	char   *buf = xstrdup (mpoint);
	const char *dir = buf + 1;
	char   *p = strrchr (dir, '/');
	const char *base = p + 1;

	if (!p)
	{
		base = dir;
		dir = p = buf;
	}
	*p = '\0';

	if (dir[0] == '/' || base[0] == '/')
		error (EXIT_FAILURE, EINVAL, "xumount: %s", mpoint);

	int     unmounted = 0;

	for (;;)
	{
		chdiruid (chroot_path);
		if (dir[0] != '\0')
			chdiruid (dir);
		safe_chdir (base, stat_permok_validator);

		if (umount2 (".", MNT_DETACH) < 0)
			break;
		unmounted = 1;
	}

	free (buf);

	if (unmounted)
		errno = 0;
	else if (errno != EINVAL)
		error (EXIT_SUCCESS, errno, "umount: %s", mpoint);

	return unmounted;
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
		if (strcmp (target, "/proc")
		    && strcmp (target, "/dev/pts")
		    && strcmp (target, "/sys"))
			error (EXIT_SUCCESS, 0,
			       "umount: %s: mount point not supported",
			       target);
		else
			unmounted |= xumount (target);
	}

	free (targets);

	if (!unmounted)
		error (EXIT_FAILURE, 0, "umount: no file systems mounted");

	return 0;
}
