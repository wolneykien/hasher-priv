
/*
  $Id$
  Copyright (C) 2003-2005  Dmitry V. Levin <ldv@altlinux.org>

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

/* Code in this file may be executed with caller privileges. */

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

#include "priv.h"
#include "xmalloc.h"

static volatile pid_t child_pid;

static int
select_retry (int n, fd_set * readfds, fd_set * writefds, fd_set * exceptfds,
	      struct timeval *timeout)
{
	int rc = -1;
	errno = EINTR;
	while (rc < 0 && errno == EINTR && child_pid)
		rc = select (n, readfds, writefds, exceptfds, timeout);
	return rc;
}

static volatile sig_atomic_t sigwinch_arrived;

static void
sigwinch_handler (__attribute__ ((unused)) int signo)
{
	++sigwinch_arrived;
}

static int child_rc;

static void
sigchld_handler (__attribute__ ((unused)) int signo)
{
	int     status;
	pid_t   child = child_pid;

	/* handle only one child */
	if (!child)
		return;
	child_pid = 0;

	if (waitpid (child, &status, 0) != child)
		error (EXIT_FAILURE, errno, "waitpid");

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
	{
		child_rc = 128 + SIGTERM;

		/* no need to kill, we have no perms,
		   and it will receive HUP anyway. */
		child_pid = 0;
	}
}

static void
wait_child (void)
{
	unsigned i;

	for (i = 0; i < 10; ++i)
		if (child_pid)
			usleep (100000);
}

static void __attribute__ ((noreturn, format (printf, 1, 0)))
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

struct io_std
{
	int     master_read_fd, master_write_fd;
	int     slave_read_fd, slave_write_fd;
	size_t  master_avail, slave_avail;
	char    master_buf[BUFSIZ], slave_buf[BUFSIZ];
};

typedef struct io_std *io_std_t;

static int pty_fd = -1, ctl_fd = -1, x11_fd = -1;
static unsigned long total_bytes_read, total_bytes_written;

static char *x11_saved_data, *x11_fake_data;

static int
handle_x11_ctl (void)
{
	x11_saved_data = xmalloc (x11_data_len);

	unsigned i;

	for (i = 0; i < x11_data_len; i++)
	{
		unsigned value = 0;

		if (sscanf (x11_key + 2 * i, "%2x", &value) != 1)
		{
			error (EXIT_SUCCESS, 0,
			       "Invalid X11 authentication data\r");
			free (x11_saved_data);
			x11_saved_data = 0;
			return -1;
		}
		x11_saved_data[i] = value;
	}

	x11_fake_data = xmalloc (x11_data_len);
	int     fd = fd_recv (ctl_fd, x11_fake_data, x11_data_len);

	if (fd >= 0)
		fd = x11_check_listen (fd);

	if (!memcmp (x11_saved_data, x11_fake_data, x11_data_len))
	{
		error (EXIT_SUCCESS, 0,
		       "Invalid X11 fake authentication data\r");
		free (x11_saved_data);
		free (x11_fake_data);
		x11_saved_data = x11_fake_data = 0;
		if (fd >= 0)
		{
			(void) close (fd);
			fd = -1;
		}
	}

	return fd;
}

static int
handle_io (io_std_t io)
{
	ssize_t n;
	fd_set  read_fds, write_fds;

	FD_ZERO (&read_fds);
	FD_ZERO (&write_fds);
	FD_SET (io->slave_read_fd, &read_fds);

	if (sigwinch_arrived)
	{
		sigwinch_arrived = 0;
		(void) tty_copy_winsize (STDIN_FILENO, pty_fd);
	}

	/* select only if child is running */
	if (child_pid)
	{
		struct timeval tmout;
		int     max_fd = io->slave_read_fd + 1, rc;

		if (io->master_read_fd >= 0 && !io->master_avail)
		{
			FD_SET (io->master_read_fd, &read_fds);
			if (io->master_read_fd > max_fd)
				max_fd = io->master_read_fd;
		}

		if (io->slave_write_fd >= 0 && io->master_avail)
		{
			FD_SET (io->slave_write_fd, &write_fds);
			if (io->slave_write_fd > max_fd)
				max_fd = io->slave_write_fd;
		}

		if (ctl_fd >= 0)
		{
			FD_SET (ctl_fd, &read_fds);
			if (ctl_fd > max_fd)
				max_fd = ctl_fd;
		}

		prepare_x11_new (&x11_fd, &max_fd, &read_fds);

		prepare_x11_select (&max_fd, &read_fds, &write_fds);

		tmout.tv_sec = wlimit.time_idle;
		tmout.tv_usec = 0;

		rc = select_retry (max_fd + 1, &read_fds, &write_fds,
				   0, wlimit.time_idle ? &tmout : 0);
		if (!rc)
			limit_exceeded
				("idle time limit (%u seconds) exceeded",
				 wlimit.time_idle);
		else if (rc < 0)
			return EXIT_FAILURE;
	}

	if (FD_ISSET (io->slave_read_fd, &read_fds))
	{
		/* handle child output */
		n = read_retry (io->slave_read_fd,
				io->slave_buf, sizeof io->slave_buf);
		if (n <= 0)
			return EXIT_FAILURE;

		if (write_loop
		    (io->master_write_fd, io->slave_buf, (size_t) n) != n)
			error (EXIT_FAILURE, errno, "write");

		total_bytes_written += n;
	}

	if (!child_pid)
		return EXIT_SUCCESS;

	if (io->slave_write_fd >= 0 && io->master_avail
	    && FD_ISSET (io->slave_write_fd, &write_fds))
	{
		/* handle child input */
		n = write_loop (io->slave_write_fd,
				io->master_buf, io->master_avail);
		if (n <= 0)
			return EXIT_FAILURE;

		if ((size_t) n < io->master_avail)
		{
			memmove (io->master_buf,
				 io->master_buf + n, io->master_avail - n);
		}

		total_bytes_read += io->master_avail;
		io->master_avail -= n;
	}

	if (io->master_read_fd >= 0 && !io->master_avail
	    && FD_ISSET (io->master_read_fd, &read_fds))
	{
		/* handle tty input */
		n = read_retry (io->master_read_fd,
				io->master_buf, sizeof io->master_buf);
		if (n > 0)
			io->master_avail = n;
		else if (n == 0)
		{
			io->master_buf[0] = 4;
			io->master_avail = 1;
		} else
			io->master_read_fd = -1;
	}

	handle_x11_select (&read_fds, &write_fds, x11_saved_data,
			   x11_fake_data);

	handle_x11_new (&x11_fd, &read_fds);

	if (ctl_fd >= 0 && FD_ISSET (ctl_fd, &read_fds))
	{
		if ((x11_fd = handle_x11_ctl ()) < 0)
		{
			x11_closedir ();
			error (EXIT_SUCCESS, 0, "X11 forwarding disabled\r");
		}
		(void) close (ctl_fd);
		ctl_fd = -1;
	}
}

static void
parent_print_progname (void)
{
	fprintf (stderr, "%s: parent: ", program_invocation_short_name);
}

int
handle_parent (pid_t a_child_pid, int a_pty_fd, int pipe_fd, int a_ctl_fd)
{
	error_print_progname = parent_print_progname;

	pty_fd = a_pty_fd;
	ctl_fd = a_ctl_fd;

	int     child_fd = use_pty ? pty_fd : pipe_fd;
	io_std_t io;

	child_pid = a_child_pid;

	struct sigaction act;

	act.sa_handler = sigchld_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP | SA_RESETHAND;
	if (sigaction (SIGCHLD, &act, 0))
		error (EXIT_FAILURE, errno, "sigaction");

	block_signal_handler (SIGCHLD, SIG_UNBLOCK);
	signal (SIGPIPE, SIG_IGN);

	io = xmalloc (sizeof (*io));
	memset (io, 0, sizeof (*io));
	io->master_read_fd = use_pty ? STDIN_FILENO : -1;
	io->master_write_fd = STDOUT_FILENO;
	io->slave_read_fd = child_fd;
	io->slave_write_fd = use_pty ? child_fd : -1;

	unblock_fd (child_fd);

	/* redirect standard descriptors, init tty if necessary */
	if (init_tty () && tty_copy_winsize (STDIN_FILENO, pty_fd) == 0)
	{
		act.sa_handler = sigwinch_handler;
		sigemptyset (&act.sa_mask);
		act.sa_flags = SA_RESTART;
		if (sigaction (SIGWINCH, &act, 0))
			error (EXIT_FAILURE, errno, "sigaction");
	}

	while (work_limits_ok (total_bytes_read, total_bytes_written))
		if (handle_io (io) != EXIT_SUCCESS)
			break;

	(void) close (pty_fd);
	wait_child ();
	dfl_signal_handler (SIGCHLD);
	forget_child ();

	return child_rc;
}
