
/*
  Copyright (C) 2003-2006  Dmitry V. Levin <ldv@altlinux.org>

  The change uid/gid module for the hasher-priv program.

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
#include <stdlib.h>
#include <unistd.h>

#include "priv.h"

#ifdef ENABLE_SETFSUGID
#include <sys/fsuid.h>

/*
 * Two setfsuid() in a row - stupid, but
 * how the hell am I supposed to check
 * whether setfsuid() succeeded or not?
 */

/* This function may be executed with root privileges. */
void
ch_uid(uid_t uid, uid_t *save)
{
	uid_t   tmp = (uid_t) setfsuid(uid);

	if (save)
		*save = tmp;
	if ((uid_t) setfsuid(uid) != uid)
		error(EXIT_FAILURE, errno, "change uid: %u", uid);
}

/* This function may be executed with root privileges. */
void
ch_gid(gid_t gid, gid_t *save)
{
	gid_t   tmp = (gid_t) setfsgid(gid);

	if (save)
		*save = tmp;
	if ((gid_t) setfsgid(gid) != gid)
		error(EXIT_FAILURE, errno, "change gid: %u", gid);
}

#else /* ! ENABLE_SETFSUGID */

/* This function may be executed with root privileges. */
void
ch_uid(uid_t uid, uid_t *save)
{
	if (save)
		*save = geteuid();
	if (setresuid((uid_t)-1, uid, 0) < 0)
		error(EXIT_FAILURE, errno, "change uid: %u", uid);
}

/* This function may be executed with root privileges. */
void
ch_gid(gid_t gid, gid_t *save)
{
	if (save)
		*save = getegid();
	if (setresgid((gid_t)-1, gid, 0) < 0)
		error(EXIT_FAILURE, errno, "change gid: %u", gid);
}

#endif /* ENABLE_SETFSUGID */
