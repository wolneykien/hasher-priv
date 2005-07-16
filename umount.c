
/*
  $Id$
  Copyright (C) 2004, 2005  Dmitry V. Levin <ldv@altlinux.org>

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
#include <mntent.h>
#include <sys/mount.h>

#include "priv.h"
#include "xmalloc.h"

#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif

static int
xumount (const char *mpoint)
{
	if (!chroot_path || chroot_path[0] != '/')
		error (EXIT_FAILURE, 0, "%s: %s", "xmount", "invalid chroot path");

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
		safe_chdir (base, stat_anyok_validator);

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

#define _PATH_MOUNTS "/proc/mounts"

int
do_umount (void)
{
	if (!allowed_mountpoints)
		error (EXIT_FAILURE, 0, "umount: no mount points allowed");

	chdiruid (chroot_path);

	char   *cwd = getcwd (0, 0);

	if (!cwd)
		error (EXIT_FAILURE, errno, "getcwd");

	unsigned cwd_len = strlen (cwd), i = 0;
	char  **v = 0;
	struct mntent *ent;
	FILE   *fp = setmntent (_PATH_MOUNTS, "r");

	if (!fp)
		error (EXIT_FAILURE, errno, "setmntent: %s", _PATH_MOUNTS);

	while ((ent = getmntent (fp)))
	{
		if (strncmp (ent->mnt_dir, cwd, cwd_len)
		    || (ent->mnt_dir[cwd_len] != '/'))
			continue;

		v = xrealloc (v, (i + 1) * sizeof (*v));
		v[i++] = xstrdup (ent->mnt_dir + cwd_len);
	}

	endmntent (fp);
	free (cwd);

	int     unmounted = 0;

	while (i > 0)
	{
		--i;
		unmounted |= xumount (v[i]);
		free (v[i]);
		v[i] = 0;
	}
	free (v);

	if (!unmounted)
		error (EXIT_FAILURE, 0, "umount: no file systems mounted");

	return 0;
}
