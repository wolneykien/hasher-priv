
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid parent handler for the hasher-priv program.

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
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>

#include "priv.h"

static  ssize_t
read_retry (int fd, void *buf, size_t count)
{
	return TEMP_FAILURE_RETRY (read (fd, buf, count));
}

static  ssize_t
write_retry (int fd, const void *buf, size_t count)
{
	return TEMP_FAILURE_RETRY (write (fd, buf, count));
}

static int
select_retry (int n, fd_set * readfds, fd_set * writefds, fd_set * exceptfds,
	      struct timeval *timeout)
{
	return TEMP_FAILURE_RETRY
		(select (n, readfds, writefds, exceptfds, timeout));
}

static int
write_loop (int fd, const char *buffer, size_t count)
{
	ssize_t offset = 0;

	while (count > 0)
	{
		ssize_t block = write_retry (fd, &buffer[offset], count);

		if (block < 0)
			return block;
		if (!block)
			break;
		offset += block;
		count -= block;
	}
	return offset;
}

static int child_rc;
static volatile pid_t child_pid;

static void
unblock_fd (int fd)
{
	int     flags;

	if ((flags = fcntl (fd, F_GETFL, 0)) < 0)
		error (EXIT_FAILURE, errno, "fcntl F_GETFL");

	flags |= O_NONBLOCK;

	if (fcntl (fd, F_SETFL, flags) < 0)
		error (EXIT_FAILURE, errno, "fcntl F_SETFL");
}

static void
sigchld_handler (int __attribute__ ((unused)) signo)
{
	int     status;
	pid_t   child = child_pid;

	/* handle only one child */
	child_pid = 0;

	if (!child)
		return;

	signal (SIGCHLD, SIG_DFL);
	if (waitpid (child, &status, 0) != child)
		error (EXIT_FAILURE, errno, "waitpid");

	child = 0;

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
		/* quite strange condition */
		child_rc = 255;
	}
}

static void
forget_child (void)
{
	if (child_pid)
		child_rc = 128 + SIGTERM;

	/* no need to kill due to EPERM */
	child_pid = 0;
}

static void
wait_child (void)
{
	unsigned i;

	for (i = 0; i < 10; ++i)
		if (child_pid)
			usleep (100000);
	forget_child ();
}

static void __attribute__ ((__noreturn__))
limit_exceeded (const char *fmt, unsigned limit)
{
	forget_child ();
	restore_tty ();
	fputc ('\n', stderr);
	error (128 + SIGTERM, 0, fmt, limit);
	exit (128 + SIGTERM);
}

static int
work_limits_ok (unsigned long bytes_read, unsigned long bytes_written)
{
	if (wlimit.bytes_read
	    && bytes_read >= (unsigned long) wlimit.bytes_read)
		limit_exceeded ("bytes read limit (%u bytes) exceeded",
				wlimit.bytes_read);

	if (wlimit.bytes_written
	    && bytes_written >= (unsigned long) wlimit.bytes_written)
		limit_exceeded ("bytes written limit (%u bytes) exceeded",
				wlimit.bytes_written);

	if (wlimit.time_elapsed)
	{
		static time_t t_start;

		if (!t_start)
			time (&t_start);
		else
		{
			time_t  t_now;

			time (&t_now);
			if (t_start + (time_t) wlimit.time_elapsed <= t_now)
				limit_exceeded
					("time elapsed limit (%u seconds) exceeded",
					 wlimit.time_elapsed);
		}
	}

	return 1;
}

int
handle_parent (pid_t child, int master)
{
	unsigned long total_bytes_read = 0, total_bytes_written = 0;
	ssize_t use_stdin, read_avail = 0;
	char    read_buf[BUFSIZ], write_buf[BUFSIZ];

	if (setgid (caller_gid) < 0)
		error (EXIT_FAILURE, errno, "setgid");

	if (setuid (caller_uid) < 0)
		error (EXIT_FAILURE, errno, "setuid");

	/* Process is no longer privileged at this point. */

	child_pid = child;

	if (signal (SIGCHLD, sigchld_handler) == SIG_ERR)
		error (EXIT_FAILURE, errno, "signal");

	block_signal_handler (SIGCHLD, SIG_UNBLOCK);

	unblock_fd (master);

	use_stdin = init_tty ();

	while (work_limits_ok (total_bytes_read, total_bytes_written))
	{
		ssize_t n;
		fd_set  read_fds, write_fds;

		FD_ZERO (&read_fds);
		FD_ZERO (&write_fds);
		FD_SET (master, &read_fds);

		/* select only if child is running */
		if (child_pid)
		{
			struct timeval tmout;
			int     rc;

			if (read_avail)
				FD_SET (master, &write_fds);
			else if (use_stdin)
				FD_SET (STDIN_FILENO, &read_fds);
			tmout.tv_sec = wlimit.time_idle;
			tmout.tv_usec = 0;

			rc = select_retry (master + 1, &read_fds, &write_fds,
					   0, wlimit.time_idle ? &tmout : 0);
			if (!rc)
				limit_exceeded
					("idle time limit (%u seconds) exceeded",
					 wlimit.time_idle);
			else if (rc < 0)
				break;
		}

		if (FD_ISSET (master, &read_fds))
		{
			/* handle child output */
			n = read_retry (master, write_buf, sizeof write_buf);
			if (n <= 0)
				break;

			if (write_loop (STDOUT_FILENO, write_buf, n) != n)
				error (EXIT_FAILURE, errno, "write");
			total_bytes_written += n;
		}

		if (!child_pid)
			continue;

		if (FD_ISSET (master, &write_fds))
		{
			/* handle child input */
			errno = 0;
			n = write_loop (master, read_buf, read_avail);
			if (n < read_avail)
			{
				if (errno != EAGAIN)
					break;
				memmove (read_buf, read_buf + n,
					 read_avail - n);
				total_bytes_read += n;
				read_avail -= n;
			} else
			{
				total_bytes_read += read_avail;
				read_avail = 0;
			}
		} else if (FD_ISSET (STDIN_FILENO, &read_fds))
		{
			/* handle tty input */
			n = read_retry (STDIN_FILENO, read_buf,
					sizeof read_buf);
			if (n > 0)
				read_avail = n;
			else if (n == 0)
			{
				read_buf[0] = 4;
				read_avail = 1;
			} else
				use_stdin = 0;
		}
	}

	wait_child ();

	return child_rc;
}
