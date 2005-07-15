
/*
  $Id$
  Copyright (C) 2004  Dmitry V. Levin <ldv@altlinux.org>

  The mount action for the hasher-priv program.

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
#include <ctype.h>
#include <grp.h>
#include <sys/mount.h>

#include "priv.h"
#include "xmalloc.h"

static void
xmount (const char *mpoint, const char *fstype, const char *grname)
{
	if (mpoint[0] != '/')
		error (EXIT_FAILURE, EINVAL, "xmount: %s", mpoint);

	struct group *gr;
	char   *options = 0;

	if (grname && (gr = getgrnam (grname)))
	{
		xasprintf (&options, "gid=%u", gr->gr_gid);
		endgrent ();
	}

	unsigned long flags = MS_MGC_VAL | MS_NOSUID;

	chdiruid (chroot_path);
	chdiruid (mpoint + 1);
	if (mount (fstype, ".", fstype, flags, options ? : "") < 0)
		error (EXIT_FAILURE, errno, "mount: %s", fstype);

	free (options);
}

int
do_mount (void)
{
	char   *targets =
		allowed_mountpoints ? xstrdup (allowed_mountpoints) : 0;
	char   *target;

	for (target = targets ? strtok (targets, " \t,") : 0; target;
	     target = strtok (0, " \t,"))
		if (!strcasecmp (target, mountpoint))
			break;

	if (!target)
		error (EXIT_FAILURE, 0,
		       "mount: %s: mount point not allowed", mountpoint);

	if (!strcmp (target, "/proc"))
		xmount (target, "proc", "proc");
	else if (!strcmp (target, "/dev/pts"))
		xmount (target, "devpts", "tty");
	else if (!strcmp (target, "/sys"))
		xmount (target, "sysfs", 0);
	else
		error (EXIT_FAILURE, 0,
		       "mount: %s: mount point not supported", target);

	free (targets);
	return 0;
}
