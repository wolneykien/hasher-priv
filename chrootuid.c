
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid actions for the hasher-priv program.

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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>

#include "priv.h"

static void
set_rlimits (void)
{
	change_rlimit_t *p;

	for (p = change_rlimit; p->name; ++p)
	{
		struct rlimit rlim;

		if (!p->hard && !p->soft)
			continue;

		if (getrlimit (p->resource, &rlim) < 0)
			error (EXIT_FAILURE, errno, "getrlimit: %s", p->name);

		if (p->hard)
			rlim.rlim_max = *(p->hard);

		if (p->soft)
			rlim.rlim_cur = *(p->soft);

		if ((unsigned long) rlim.rlim_max <
		    (unsigned long) rlim.rlim_cur)
			rlim.rlim_cur = rlim.rlim_max;

		if (setrlimit (p->resource, &rlim) < 0)
			error (EXIT_FAILURE, errno, "setrlimit: %s", p->name);
	}
}

static int
handle_child (uid_t uid, gid_t gid, char *const *env, int *out)
{
	if (setgid (gid) < 0)
		error (EXIT_FAILURE, errno, "setgid");

	if (setuid (uid) < 0)
		error (EXIT_FAILURE, errno, "setuid");

	/* Process is no longer privileged at this point. */

	if (nice (change_nice) < 0)
		error (EXIT_FAILURE, errno, "nice");

	/* This system call always succeeds. */
	umask (change_umask);

	dfl_signal_handler (SIGPIPE);
	dfl_signal_handler (SIGTERM);

	if (dup2 (out[1], STDOUT_FILENO) != STDOUT_FILENO ||
	    dup2 (out[1], STDERR_FILENO) != STDERR_FILENO)
		error (EXIT_FAILURE, errno, "dup2");

	if (close (out[0]) < 0 || close (out[1]) < 0)
		error (EXIT_FAILURE, errno, "close");

	execve (chroot_argv[0], (char *const *) chroot_argv, env);
	error (EXIT_FAILURE, errno, "chrootuid: execve: %s", chroot_argv[0]);
	return EXIT_FAILURE;
}

static int
chrootuid (uid_t uid, gid_t gid, const char *ehome,
	   const char *euser, const char *epath)
{
	const char *const env[] =
		{ ehome, euser, epath, "SHELL=/bin/sh", "TERM=dumb", 0 };
	int     out[2];
	pid_t   pid;

	if (uid < MIN_CHANGE_UID || uid == getuid ())
		error (EXIT_FAILURE, 0, "chrootuid: invalid uid: %u", uid);

	chdiruid (chroot_path);

	endpwent ();
	endgrent ();

	/* Check and sanitize file descriptors again. */
	sanitize_fds ();

	if (pipe (out) < 0)
		error (EXIT_FAILURE, errno, "pipe");

	if (chroot (".") < 0)
		error (EXIT_FAILURE, errno, "chroot: %s", chroot_path);

	if (setgroups (0, 0) < 0)
		error (EXIT_FAILURE, errno, "setgroups");

	set_rlimits ();

	block_signal_handler (SIGCHLD, SIG_BLOCK);

	if ((pid = fork ()) < 0)
		error (EXIT_FAILURE, errno, "fork");

	return pid ? handle_parent (pid, out) :
		handle_child (uid, gid, (char *const *) env, out);
}

int
do_chrootuid1 (void)
{
	return chrootuid (change_uid1, change_gid1,
			  "HOME=/root", "USER=root",
			  "PATH=/sbin:/usr/sbin:/bin:/usr/bin");
}

int
do_chrootuid2 (void)
{
	return chrootuid (change_uid2, change_gid2,
			  "HOME=/usr/src", "USER=builder",
			  "PATH=/bin:/usr/bin:/usr/X11R6/bin");
}
