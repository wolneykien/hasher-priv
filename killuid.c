
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

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

static int
killuid (uid_t uid)
{
	if (uid < MIN_CHANGE_UID || uid == getuid ())
		error (EXIT_FAILURE, 0, "killuid: invalid uid: %u", uid);

	if (prctl (PR_SET_DUMPABLE, 0) && !__libc_enable_secure)
		error (EXIT_FAILURE, errno, "killuid: prctl PR_SET_DUMPABLE");

	if (setuid (uid) < 0)
		error (EXIT_FAILURE, errno, "killuid: setuid");

	if (kill (-1, SIGKILL))
		error (EXIT_FAILURE, errno, "killuid: kill");

	purge_ipc (uid);

	return 0;
};

int
do_killuid1 (void)
{
	return killuid (change_uid1);
}

int
do_killuid2 (void)
{
	return killuid (change_uid2);
}
