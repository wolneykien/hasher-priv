
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  The chdir-with-verfification module for the hasher-priv program.

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
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include "priv.h"

#define       ENABLE_SUPPLEMENTARY_GROUPS

#ifdef ENABLE_SETFSUGID
#include <sys/fsuid.h>

/*
 * Two setfsuid() in a row - stupid, but
 * how the hell am I supposed to check
 * whether setfsuid() succeeded?
 */
static void
ch_uid (uid_t uid, uid_t * save)
{
	uid_t   tmp = setfsuid (uid);

	if (save)
		*save = tmp;
	if ((uid_t) setfsuid (uid) != uid)
		error (EXIT_FAILURE, errno, "change uid: %u", uid);
}

static void
ch_gid (gid_t gid, gid_t * save)
{
	gid_t   tmp = setfsgid (gid);

	if (save)
		*save = tmp;
	if ((gid_t) setfsgid (gid) != gid)
		error (EXIT_FAILURE, errno, "change gid: %u", gid);
}

#else /* ! ENABLE_SETFSUGID */

static void
ch_uid (uid_t uid, uid_t * save)
{
	if (save)
		*save = geteuid ();
	if (setreuid (-1, uid) < 0)
		error (EXIT_FAILURE, errno, "change uid: %u", uid);
}

static void
ch_gid (gid_t gid, gid_t * save)
{
	if (save)
		*save = getegid ();
	if (setregid (-1, gid) < 0)
		error (EXIT_FAILURE, errno, "change gid: %u", gid);
}

#endif /* ENABLE_SETFSUGID */

static int
is_not_prefix (const char *prefix, const char *sample)
{
	unsigned len = strlen (prefix);

	return strncmp (sample, prefix, len)
		|| ((sample[len] != '\0') && (sample[len] != '/'));
}

/*
 * Change the current work directory to the given path.
 */
void
chdiruid (const char *path, chdiruid_t type)
{
	uid_t   saved_uid = -1;
	gid_t   saved_gid = -1;
	char   *cwd;
	struct stat st;

	if (!path || ((type == CHDIRUID_ABSOLUTE) && (*path != '/')))
		error (EXIT_FAILURE, 0, "chdiruid: invalid path");

	/* Set credentials. */
#ifdef ENABLE_SUPPLEMENTARY_GROUPS
	if (initgroups (caller_user, caller_gid) < 0)
		error (EXIT_FAILURE, errno, "initgroups: %s", caller_user);
#endif /* ENABLE_SUPPLEMENTARY_GROUPS */
	ch_gid (caller_gid, &saved_gid);
	ch_uid (caller_uid, &saved_uid);

	/* Change and verify directory. */
	if (chdir (path) < 0)
		error (EXIT_FAILURE, errno, "chdir: %s", path);

	if (!(cwd = get_current_dir_name ()))
		error (EXIT_FAILURE, errno, "getcwd");

	if ((type == CHDIRUID_ABSOLUTE) && chroot_prefix && *chroot_prefix
	    && is_not_prefix (strcmp (chroot_prefix,
				      "~") ? chroot_prefix : caller_home,
			      cwd))
		error (EXIT_FAILURE, 0, "%s: prefix mismatch", cwd);

	if (stat (".", &st) < 0)
		error (EXIT_FAILURE, errno, "stat: %s", cwd);

	if (st.st_uid != caller_uid)
		error (EXIT_FAILURE, 0,
		       "%s: expected owner %u, found owner %u", cwd,
		       caller_uid, st.st_uid);

	if (st.st_gid != change_gid1)
		error (EXIT_FAILURE, 0,
		       "%s: expected group %u, found group %u", cwd,
		       change_gid1, st.st_gid);

	if ((st.st_mode & S_IWOTH)
	    || ((st.st_mode & S_IWGRP) && !(st.st_mode & S_ISVTX)))
		error (EXIT_FAILURE, 0, "%s: bad perms: %o", cwd,
		       st.st_mode & 07777);

	free (cwd);

	/* Restore credentials. */
	ch_uid (saved_uid, 0);
	ch_gid (saved_gid, 0);
#ifdef ENABLE_SUPPLEMENTARY_GROUPS
	if (setgroups (0, 0) < 0)
		error (EXIT_FAILURE, errno, "setgroups");
#endif /* ENABLE_SUPPLEMENTARY_GROUPS */
}
