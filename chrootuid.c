
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

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
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>

#include "priv.h"

static int
write_loop (int fd, const char *buffer, size_t count)
{
	ssize_t offset = 0;

	while (count > 0)
	{
		ssize_t block = write (fd, &buffer[offset], count);

		if (block < 0)
		{
			if (errno == EINTR)
				continue;
			return block;
		}
		if (!block)
			break;
		offset += block;
		count -= block;
	}
	return offset;
}

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

static void
block_signal_handler (int no, int what)
{
	sigset_t unblock;

	sigemptyset (&unblock);
	sigaddset (&unblock, no);
	if (sigprocmask (what, &unblock, 0) < 0)
		error (EXIT_FAILURE, errno, "sigprocmask");
}

static void
dfl_signal_handler (int no)
{
	if (signal (no, SIG_DFL) < 0)
		error (EXIT_FAILURE, errno, "signal");

	block_signal_handler (no, SIG_UNBLOCK);
}

static int
child (uid_t uid, gid_t gid, char *const *env, int *out)
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

static pid_t child_pid;
static int child_rc;
static int in = -1;

static void
forget_and_unblock (void)
{
	int     flags;

	child_pid = 0;

	if ((flags = fcntl (in, F_GETFL, 0)) < 0)
		error (EXIT_FAILURE, errno, "fcntl F_GETFL");

	flags |= O_NONBLOCK;

	if (fcntl (in, F_SETFL, flags) < 0)
		error (EXIT_FAILURE, errno, "fcntl F_SETFL");
}

static void
sigchld_handler (int signo)
{
	int     status;

	if (!child_pid)
		return;

	signal (SIGCHLD, SIG_IGN);
	if (waitpid (child_pid, &status, 0) != child_pid)
		error (EXIT_FAILURE, errno, "waitpid");

	forget_and_unblock ();

	if (WIFEXITED (status))
	{
		if (WEXITSTATUS (status))
		{
			child_rc = WEXITSTATUS (status);
			return;
		}
		return;
	} else if (WIFSIGNALED (status))
	{
		child_rc = 128 + WTERMSIG (status);
		return;
	} else
	{
		/* quite strange */
		child_rc = 255;
	}
}

static void
kill_and_forget (void)
{
	pid_t   pid = child_pid;

	forget_and_unblock ();
	if (pid)
	{
		kill (pid, SIGTERM);
		child_rc = 128 + SIGTERM;
	}
}

static int
work_limits_ok (unsigned long bytes_written)
{
	if (wlimit.bytes_written
	    && bytes_written >= (unsigned long) wlimit.bytes_written)
	{
		kill_and_forget ();
		error (128 + SIGTERM, 0,
		       "bytes written limit (%u bytes) exceeded",
		       wlimit.bytes_written);
	}
	if (wlimit.time_elapsed)
	{
		static time_t time_start;

		if (!time_start)
			time (&time_start);
		else
		{
			time_t  time_now;

			time (&time_now);
			if (time_start + (time_t) wlimit.time_elapsed <=
			    time_now)
			{
				kill_and_forget ();
				error (128 + SIGTERM, 0,
				       "time elapsed limit (%u seconds) exceeded",
				       wlimit.time_elapsed);
			}
		}
	}

	return 1;
}

static int
parent (int *out)
{
	unsigned long bytes_read = 0;
	unsigned i;

	if (setgid (caller_gid) < 0)
		error (EXIT_FAILURE, errno, "setgid");

	if (setuid (caller_uid) < 0)
		error (EXIT_FAILURE, errno, "setuid");

	/* Process is no longer privileged at this point. */

	if (close (out[1]) < 0)
		error (EXIT_FAILURE, errno, "close");

	in = out[0];

	if (signal (SIGCHLD, sigchld_handler) < 0)
		error (EXIT_FAILURE, errno, "signal");

	block_signal_handler (SIGCHLD, SIG_UNBLOCK);

	while (work_limits_ok (bytes_read))
	{
		char    buffer[BUFSIZ];
		ssize_t n;

		if (child_pid && wlimit.time_idle)
		{
			fd_set  set;
			struct timeval timeout;
			int     rc;

			FD_ZERO (&set);
			FD_SET (in, &set);
			timeout.tv_sec = wlimit.time_idle;
			timeout.tv_usec = 0;

			rc = select (in + 1, &set, 0, 0, &timeout);
			if (!rc)
			{
				kill_and_forget ();
				error (128 + SIGTERM, 0,
				       "idle time limit (%u seconds) exceeded",
				       wlimit.time_idle);
			} else if (rc < 0 && errno == EINTR)
				continue;
		}
		if ((n = read (in, buffer, sizeof buffer)) < 0)
		{
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
				break;
			error (EXIT_FAILURE, errno, "read");
		}
		if (!n)
			break;

		bytes_read += n;
		if (write_loop (STDOUT_FILENO, buffer, n) != n)
			error (EXIT_FAILURE, errno, "write");
	}

	for (i = 0; i < 10; ++i)
		if (child_pid)
			usleep (100000);

	if (child_pid)
		kill_and_forget ();

	return child_rc;
}

static int
chrootuid (const char *name, uid_t uid, gid_t gid, const char *ehome,
	   const char *euser, const char *epath)
{
	const char *const env[] =
		{ ehome, euser, epath, "SHELL=/bin/sh", "TERM=dumb", 0 };
	int     out[2];

	if (uid < MIN_CHANGE_UID || uid == getuid ())
		error (EXIT_FAILURE, 0, "chrootuid: invalid uid: %u", uid);

	chdiruid (chroot_path, CHDIRUID_ABSOLUTE);

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

	if ((child_pid = fork ()) < 0)
		error (EXIT_FAILURE, errno, "fork");

	return child_pid ? parent (out) : child (uid, gid,
						 (char *const *) env, out);
}

int
do_chrootuid1 (void)
{
	return chrootuid (change_user1, change_uid1, change_gid1,
			  "HOME=/root", "USER=root",
			  "PATH=/sbin:/usr/sbin:/bin:/usr/bin");
}

int
do_chrootuid2 (void)
{
	return chrootuid (change_user2, change_uid2, change_gid2,
			  "HOME=/usr/src", "USER=builder",
			  "PATH=/bin:/usr/bin:/usr/X11R6/bin");
}
