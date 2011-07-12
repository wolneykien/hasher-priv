
/*
  Copyright (C) 2003-2005  Dmitry V. Levin <ldv@altlinux.org>

  The killuid actions for the hasher-priv program.

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

/* Code in this file may be executed with root privileges. */

#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "priv.h"

#ifndef	PR_SET_DUMPABLE
#define	PR_SET_DUMPABLE 4
#endif

extern int __libc_enable_secure;

int
do_killuid(void)
{
	uid_t u = getuid();

	if (change_uid1 < MIN_CHANGE_UID || change_uid1 == u)
		error(EXIT_FAILURE, 0, "killuid: invalid uid: %u", change_uid1);
	if (change_uid2 < MIN_CHANGE_UID || change_uid2 == u)
		error(EXIT_FAILURE, 0, "killuid: invalid uid: %u", change_uid2);

	if (prctl(PR_SET_DUMPABLE, 0) && !__libc_enable_secure)
		error(EXIT_FAILURE, errno, "killuid: prctl PR_SET_DUMPABLE");

	if (setreuid(change_uid1, change_uid2) < 0)
		error(EXIT_FAILURE, errno, "killuid: setreuid");

	if (kill(-1, SIGKILL))
		error(EXIT_FAILURE, errno, "killuid: kill");

	purge_ipc(change_uid1, change_uid2);

	return 0;
}
