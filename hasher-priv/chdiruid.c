
/*
  Copyright (C) 2003-2006  Dmitry V. Levin <ldv@altlinux.org>

  The switch-user-and-chdir-with-validation module for the hasher-priv program.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>

#include "priv.h"
#include "xmalloc.h"

/*
 * Check whether the file path PREFIX is prefix of the file path SAMPLE.
 * Return zero if prefix check succeeded, and non-zero otherwise.
 */

/* This function may be executed with caller privileges. */
static int
is_not_prefix(const char *prefix, const char *sample)
{
	size_t  len = strlen(prefix);

	return strncmp(sample, prefix, len)
		|| ((sample[len] != '\0') && (sample[len] != '/'));
}

/*
 * Change the current work directory to the given path.
 * If chroot_prefix is set, ensure that it is prefix of the given path.
 */

/* This function may be executed with root privileges. */
static void
chdiruid_simple(const char *path)
{
	/* Change and verify directory. */
	safe_chdir(path, stat_userok_validator);

	char   *cwd;

	if (!(cwd = getcwd(0, 0UL)))
		error(EXIT_FAILURE, errno, "getcwd");

	/* Check for chroot_prefix. */
	if ((chroot_prefix && *chroot_prefix
	     && is_not_prefix(chroot_prefix, cwd)))
		error(EXIT_FAILURE, 0,
		      "%s: prefix mismatch, working directory should start with %s",
		      cwd, chroot_prefix);

	free(cwd);
}

/*
 * Change the current work directory to the given path.
 * Temporary change credentials to caller_user during this operation.
 * If the path is relative, chdir to each path element sequentially.
 * If chroot_prefix is set, ensure that it is prefix of the given path.
 */

/* This function may be executed with root privileges. */
void
chdiruid(const char *path)
{
	uid_t   saved_uid = (uid_t) - 1;
	gid_t   saved_gid = (gid_t) - 1;

	if (!path)
		error(EXIT_FAILURE, 0, "chdiruid: invalid chroot path");

	/* Set credentials. */
#ifdef ENABLE_SUPPLEMENTARY_GROUPS
	if (initgroups(caller_user, caller_gid) < 0)
		error(EXIT_FAILURE, errno, "initgroups: %s", caller_user);
#endif /* ENABLE_SUPPLEMENTARY_GROUPS */
	ch_gid(caller_gid, &saved_gid);
	ch_uid(caller_uid, &saved_uid);

	/* Change and verify directory, check for chroot_prefix. */
	if (path[0] == '/' || !strchr(path, '/'))
		chdiruid_simple(path);
	else
	{
		char   *elem, *p = xstrdup(path);

		for (elem = strtok(p, "/"); elem; elem = strtok(0, "/"))
			chdiruid_simple(elem);
		free(p);
	}

	/* Restore credentials. */
	ch_uid(saved_uid, 0);
	ch_gid(saved_gid, 0);
#ifdef ENABLE_SUPPLEMENTARY_GROUPS
	if (setgroups(0UL, 0) < 0)
		error(EXIT_FAILURE, errno, "setgroups");
#endif /* ENABLE_SUPPLEMENTARY_GROUPS */
}
