
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  Command line parser for the pkg-build-priv program.

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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "priv.h"

static void __attribute__ ((__noreturn__)) usage (int rc)
{
	fprintf (stderr,
		 "%s: privileged pkg-build helper.\n"
		 "Usage: %s <args>\n"
		 "       where <args> are any of:\n"
		 "getugid1:\n"
		 "       print uid:gid pair for user1\n"
		 "killuid1:\n"
		 "       kill all processes of user1\n"
		 "chrootuid1 <chroot path> <program> [program args]:\n"
		 "       execute program in given chroot with credentials of user1\n"
		 "getugid2:\n"
		 "       print uid:gid pair for user2\n"
		 "killuid2:\n"
		 "       kill all processes of user2\n"
		 "chrootuid2 <chroot path> <program> [program args]:\n"
		 "       execute program in given chroot with credentials of user2\n"
		 "makedev <chroot path>:\n"
		 "       make devices in given chroot\n"
		 "-h or --help: print this help text and exit.\n",
		 __progname, __progname);
	exit (rc);
}

const char *chroot_path, *chroot_prefix;
const char **chroot_argv;

/* Parse command line arguments. */
task_t
parse_cmdline (int ac, const char *av[])
{
	if (ac < 2)
		usage (EXIT_FAILURE);

	if (!strcmp ("-h", av[1]) || !strcmp ("--help", av[1]))
		usage (EXIT_SUCCESS);
	else if (!strcmp ("getugid1", av[1]))
	{
		if (ac != 2)
			usage (EXIT_FAILURE);
		return TASK_GETUGID1;
	} else if (!strcmp ("killuid1", av[1]))
	{
		if (ac != 2)
			usage (EXIT_FAILURE);
		return TASK_KILLUID1;
	} else if (!strcmp ("chrootuid1", av[1]))
	{
		if (ac < 4)
			usage (EXIT_FAILURE);
		chroot_path = av[2];
		if (*chroot_path != '/')
			error (EXIT_FAILURE, 0, "absolute pathname required");
		chroot_argv = av + 3;
		return TASK_CHROOTUID1;
	} else if (!strcmp ("getugid2", av[1]))
	{
		if (ac != 2)
			usage (EXIT_FAILURE);
		return TASK_GETUGID2;
	} else if (!strcmp ("killuid2", av[1]))
	{
		if (ac != 2)
			usage (EXIT_FAILURE);
		return TASK_KILLUID2;
	} else if (!strcmp ("chrootuid2", av[1]))
	{
		if (ac < 4)
			usage (EXIT_FAILURE);
		chroot_path = av[2];
		if (*chroot_path != '/')
			error (EXIT_FAILURE, 0, "absolute pathname required");
		chroot_argv = av + 3;
		return TASK_CHROOTUID2;
	} else if (!strcmp ("makedev", av[1]))
	{
		if (ac != 3)
			usage (EXIT_FAILURE);
		chroot_path = av[2];
		return TASK_MAKEDEV;
	} else
		usage (EXIT_FAILURE);
}
