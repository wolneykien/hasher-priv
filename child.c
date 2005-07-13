
/*
  $Id$
  Copyright (C) 2004-2005  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid child handler for the hasher-priv program.

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

/* Code in this file may be executed with child privileges. */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "priv.h"

static void
connect_fds (int pty_fd, int pipe_fd)
{
	int     fd = use_pty ? pty_fd : pipe_fd;

	if (setsid () < 0)
		error (EXIT_FAILURE, errno, "setsid");

	if (ioctl (pty_fd, TIOCSCTTY, 0) < 0)
		error (EXIT_FAILURE, errno, "ioctl TIOCSCTTY");

	if (use_pty)
	{
		if (dup2 (pty_fd, STDIN_FILENO) < 0)
			error (EXIT_FAILURE, errno, "dup2");
	} else
	{
		/* redirect stdin to /dev/null if and only if
		   use_pty is not set and stdin is a tty */
		if (isatty (STDIN_FILENO))
			nullify_stdin ();
	}
	if (dup2 (fd, STDOUT_FILENO) < 0 || dup2 (fd, STDERR_FILENO) < 0)
		error (EXIT_FAILURE, errno, "dup2");

	if (pty_fd > STDERR_FILENO)
		close (pty_fd);
	if (pipe_fd > STDERR_FILENO)
		close (pipe_fd);
}

static int
xauth_add_entry (char *const *env)
{
	if (!x11_display || !x11_key)
		return EXIT_FAILURE;

	const char *av[] = { "xauth", "add", ":10.0", ".", x11_key, 0 };
	const char *path = "/usr/X11R6/bin/xauth";

	pid_t   pid = fork ();

	if (pid < 0)
		return EXIT_FAILURE;

	if (!pid)
	{
		execve (path, (char *const *) av, env);
		error (EXIT_SUCCESS, errno, "execve: %s", path);
		_exit (EXIT_FAILURE);
	} else
	{
		int     status = 0;

		if (waitpid (pid, &status, 0) != pid || !WIFEXITED (status))
			return 1;
		return WEXITSTATUS (status);
	}
}

static void
child_print_progname (void)
{
	fprintf (stderr, "%s: child: ", program_invocation_short_name);
}

int
handle_child (char *const *env, int pty_fd, int pipe_fd, int ctl_fd)
{
	error_print_progname = child_print_progname;

	connect_fds (pty_fd, pipe_fd);

	dfl_signal_handler (SIGHUP);
	dfl_signal_handler (SIGPIPE);
	dfl_signal_handler (SIGTERM);

	if (nice (change_nice) < 0)
		error (EXIT_FAILURE, errno, "nice");

	if (ctl_fd >= 0)
	{
		int x11_fd = x11_listen ();

		if (x11_fd >= 0)
		{
			if (xauth_add_entry (env) == EXIT_SUCCESS)
				fd_send (ctl_fd, x11_fd);
			(void) close (x11_fd);
		}
	}

	umask (change_umask);

	block_signal_handler (SIGCHLD, SIG_UNBLOCK);

	execve (chroot_argv[0], (char *const *) chroot_argv, env);
	error (EXIT_FAILURE, errno, "chrootuid: execve: %s", chroot_argv[0]);
	return EXIT_FAILURE;
}
