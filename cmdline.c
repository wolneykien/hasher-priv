
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

  Command line parser for the hasher-priv program.

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
#include <limits.h>

#include "priv.h"

static void __attribute__ ((__noreturn__)) usage (int rc)
{
	fprintf ((rc == EXIT_SUCCESS) ? stdout : stderr,
		 "Usage: %s [options] <args>\n"
		 "Privileged hasher helper.\n"
		 "\nThis program is free software, covered by the GNU General Public License;\n"
		 "hasher-priv comes with ABSOLUTELY NO WARRANTY, see license for details.\n"
		 "\nValid options are:\n\n"
		 "-<number>:\n"
		 "       subconfig identifier;\n"
		 "--version:\n"
		 "       print program version and exit.\n"
		 "-h or --help:\n"
		 "       print this help text and exit.\n"
		 "\nValid args are any of:\n\n"
		 "getugid1:\n"
		 "       print uid:gid pair for user1;\n"
		 "killuid1:\n"
		 "       kill all processes of user1;\n"
		 "chrootuid1 <chroot path> <program> [program args]:\n"
		 "       execute program in given chroot with credentials of user1;\n"
		 "getugid2:\n"
		 "       print uid:gid pair for user2;\n"
		 "killuid2:\n"
		 "       kill all processes of user2;\n"
		 "chrootuid2 <chroot path> <program> [program args]:\n"
		 "       execute program in given chroot with credentials of user2;\n"
		 "makedev <chroot path>:\n"
		 "       make devices in given chroot;\n"
		 "mount <chroot path> <fs type>:\n"
		 "       mount filesystem of given type;\n"
		 "umount <chroot path>:\n"
		 "       umount all filesystems.\n", __progname);
	exit (rc);
}

static void __attribute__ ((__noreturn__)) print_version (void)
{
	printf ("hasher-priv version %s\n", PROJECT_VERSION);
	exit (EXIT_SUCCESS);
}

const char *chroot_path;
const char *mount_fstype;
const char **chroot_argv;
unsigned caller_num;

static unsigned
get_caller_num (const char *str)
{
	char   *p = 0;
	unsigned long n;

	if (!*str)
		error (EXIT_FAILURE, 0, "invalid option: -%s", str);

	n = strtoul (str, &p, 10);
	if (!p || *p || !n || n > INT_MAX)
		error (EXIT_FAILURE, 0, "invalid option: -%s", str);

	return n;
}

/* Parse command line arguments. */
task_t
parse_cmdline (int argc, const char *argv[])
{
	int     ac;
	const char **av;

	if (argc < 2)
		usage (EXIT_FAILURE);

	ac = argc - 1;
	av = argv + 1;

	if (av[0][0] == '-')
	{
		/* option */
		if (!strcmp ("-h", av[0]) || !strcmp ("--help", av[0]))
			usage (EXIT_SUCCESS);

		if (!strcmp ("--version", av[0]))
			print_version ();

		caller_num = get_caller_num (&av[0][1]);
		--ac;
		++av;
	}

	if (!strcmp ("getugid1", av[0]))
	{
		if (ac != 1)
			usage (EXIT_FAILURE);
		return TASK_GETUGID1;
	} else if (!strcmp ("killuid1", av[0]))
	{
		if (ac != 1)
			usage (EXIT_FAILURE);
		return TASK_KILLUID1;
	} else if (!strcmp ("chrootuid1", av[0]))
	{
		if (ac < 3)
			usage (EXIT_FAILURE);
		chroot_path = av[1];
		if (*chroot_path != '/')
			error (EXIT_FAILURE, 0, "absolute pathname required");
		chroot_argv = av + 2;
		return TASK_CHROOTUID1;
	} else if (!strcmp ("getugid2", av[0]))
	{
		if (ac != 1)
			usage (EXIT_FAILURE);
		return TASK_GETUGID2;
	} else if (!strcmp ("killuid2", av[0]))
	{
		if (ac != 1)
			usage (EXIT_FAILURE);
		return TASK_KILLUID2;
	} else if (!strcmp ("chrootuid2", av[0]))
	{
		if (ac < 3)
			usage (EXIT_FAILURE);
		chroot_path = av[1];
		if (*chroot_path != '/')
			error (EXIT_FAILURE, 0, "absolute pathname required");
		chroot_argv = av + 2;
		return TASK_CHROOTUID2;
	} else if (!strcmp ("makedev", av[0]))
	{
		if (ac != 2)
			usage (EXIT_FAILURE);
		chroot_path = av[1];
		return TASK_MAKEDEV;
	} else if (!strcmp ("mount", av[0]))
	{
		if (ac != 3)
			usage (EXIT_FAILURE);
		chroot_path = av[1];
		mount_fstype = av[2];
		return TASK_MOUNT;
	} else if (!strcmp ("umount", av[0]))
	{
		if (ac != 2)
			usage (EXIT_FAILURE);
		chroot_path = av[1];
		return TASK_UMOUNT;
	} else
		usage (EXIT_FAILURE);
}
