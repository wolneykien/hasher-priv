
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

  The chdir-with-validation module for the hasher-priv program.

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
#include <stdlib.h>
#include <unistd.h>

#include "priv.h"

/*
 * Change the current working directory.
 * Call validator before chdir.
 */
void
safe_chdir (const char *name, VALIDATE_FPTR validator)
{
	struct stat st, st2;

	if (lstat (name, &st) < 0)
		error (EXIT_FAILURE, errno, "lstat: %s", name);

	if (!S_ISDIR (st.st_mode))
		error (EXIT_FAILURE, ENOTDIR, "%s", name);

	validator (&st, name);

	if (chdir (name) < 0)
		error (EXIT_FAILURE, errno, "chdir: %s", name);

	if (lstat (".", &st2) < 0)
		error (EXIT_FAILURE, errno, "lstat: %s", name);

	if (st.st_dev != st2.st_dev ||
	    st.st_ino != st2.st_ino ||
	    st.st_mode != st2.st_mode ||
	    st.st_uid != st2.st_uid ||
	    st.st_gid != st2.st_gid || st.st_rdev != st2.st_rdev)
		error (EXIT_FAILURE, 0, "%s: changed during execution", name);
}

void
stat_userok_validator (struct stat *st, const char *name)
{
	if (st->st_uid != caller_uid)
		error (EXIT_FAILURE, 0,
		       "%s: expected owner %u, found owner %u",
		       name, caller_uid, st->st_uid);

	if (st->st_gid != change_gid1)
		error (EXIT_FAILURE, 0,
		       "%s: expected group %u, found group %u",
		       name, change_gid1, st->st_gid);

	if ((st->st_mode & S_IWOTH)
	    || ((st->st_mode & S_IWGRP) && !(st->st_mode & S_ISVTX)))
		error (EXIT_FAILURE, 0,
		       "%s: bad perms: %o", name, st->st_mode & 07777);
}

void
stat_rootok_validator (struct stat *st, const char *name)
{
	if (st->st_uid)
		error (EXIT_FAILURE, 0, "%s: bad owner: %u", name,
		       st->st_uid);

	if (st->st_mode & (S_IWGRP | S_IWOTH))
		error (EXIT_FAILURE, 0, "%s: bad perms: %o", name,
		       st->st_mode & 07777);
}

void
stat_permok_validator (struct stat *st, const char *name)
{
	if (st->st_uid && st->st_uid != caller_uid)
		error (EXIT_FAILURE, 0,
		       "%s: expected owner 0 or %u, found owner %u",
		       name, caller_uid, st->st_uid);

	if (st->st_mode & (S_IWGRP | S_IWOTH))
		error (EXIT_FAILURE, 0, "%s: bad perms: %o", name,
		       st->st_mode & 07777);
}
