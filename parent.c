
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
#include <pwd.h>
#include <grp.h>

#include "priv.h"
#include "xmalloc.h"

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

		if (block <= 0)
			return offset ?: block;
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

static volatile sig_atomic_t sigwinch_arrived;

static void
sigwinch_handler (__attribute__ ((unused)) int signo)
{
	++sigwinch_arrived;
}

static volatile sig_atomic_t x11_accept_timed_out;

static void
sigalrm_handler (__attribute__ ((unused)) int signo)
{
	++x11_accept_timed_out;
	signal (SIGALRM, SIG_DFL);
}

static void
sigchld_handler (__attribute__ ((unused)) int signo)
{
	int     status;
	pid_t   child = child_pid;

	/* handle only one child */
	if (!child)
		return;
	child_pid = 0;
	signal (SIGCHLD, SIG_DFL);

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

struct io_pair
{
	int     master_read_fd, master_write_fd;
	int     slave_read_fd, slave_write_fd;
	size_t  master_read_avail, slave_read_avail;
	char    master_read_buf[BUFSIZ], slave_read_buf[BUFSIZ];
};

typedef struct io_pair *io_pair_t;

static io_pair_t *io_x11_list;
static unsigned io_x11_count;

static  io_pair_t
io_x11_new (int master_fd, int slave_fd)
{
	unsigned i;

	for (i = 0; i < io_x11_count; ++i)
		if (!io_x11_list[i])
			break;

	if (i == io_x11_count)
		io_x11_list =
			xrealloc (io_x11_list,
				  (++io_x11_count) * sizeof (*io_x11_list));

	io_pair_t io = io_x11_list[i] = xmalloc (sizeof (*io_x11_list[i]));

	memset (io, 0, sizeof (*io));
	io->master_read_fd = io->master_write_fd = master_fd;
	io->slave_read_fd = io->slave_write_fd = slave_fd;
	unblock_fd (master_fd);
	unblock_fd (slave_fd);

	return io;
}

static void
io_x11_free (io_pair_t io)
{
	unsigned i;

	for (i = 0; i < io_x11_count; ++i)
		if (io_x11_list[i] == io)
			break;
	if (i == io_x11_count)
		error (EXIT_FAILURE, 0,
		       "io_x11_free: entry %p not found, count=%u\n", io,
		       io_x11_count);
	io_x11_list[i] = 0;

	(void) close (io->master_read_fd);
	if (io->master_read_fd != io->master_write_fd)
		(void) close (io->master_write_fd);
	(void) close (io->slave_read_fd);
	if (io->slave_read_fd != io->slave_write_fd)
		(void) close (io->slave_write_fd);
	memset (io, 0, sizeof (*io));
	free (io);
}

static unsigned long total_bytes_read, total_bytes_written;

static int pty_fd = -1, x11_fd = -1;

static int
handle_io (io_pair_t std_io)
{
	ssize_t n;
	unsigned i;
	fd_set  read_fds, write_fds;

	FD_ZERO (&read_fds);
	FD_ZERO (&write_fds);
	FD_SET (std_io->slave_read_fd, &read_fds);

	if (sigwinch_arrived)
	{
		sigwinch_arrived = 0;
		(void) tty_copy_winsize (STDIN_FILENO, pty_fd);
	}

	/* select only if child is running */
	if (child_pid)
	{
		struct timeval tmout;
		int     max_fd = std_io->slave_read_fd + 1, rc;

		if (std_io->master_read_fd >= 0 && !std_io->master_read_avail)
		{
			FD_SET (std_io->master_read_fd, &read_fds);
			if (std_io->master_read_fd > max_fd)
				max_fd = std_io->master_read_fd;
		}
		if (std_io->slave_write_fd >= 0 && std_io->master_read_avail)
		{
			FD_SET (std_io->slave_write_fd, &write_fds);
			if (std_io->slave_write_fd > max_fd)
				max_fd = std_io->slave_write_fd;
		}
		if (x11_fd >= 0)
		{
			FD_SET (x11_fd, &read_fds);
			if (x11_fd > max_fd)
				max_fd = x11_fd;
		}

		for (i = 0; i < io_x11_count; ++i)
		{
			io_pair_t io;

			if (!(io = io_x11_list[i]))
				continue;

			if (io->slave_read_fd >= 0 && !io->slave_read_avail)
			{
				FD_SET (io->slave_read_fd, &read_fds);
				if (io->slave_read_fd > max_fd)
					max_fd = io->slave_read_fd;
			}
			if (io->master_read_fd >= 0 && !io->master_read_avail)
			{
				FD_SET (io->master_read_fd, &read_fds);
				if (io->master_read_fd > max_fd)
					max_fd = io->master_read_fd;
			}
			if (io->slave_write_fd >= 0 && io->master_read_avail)
			{
				FD_SET (io->slave_write_fd, &write_fds);
				if (io->slave_write_fd > max_fd)
					max_fd = io->slave_write_fd;
			}
			if (io->master_write_fd >= 0 && io->slave_read_avail)
			{
				FD_SET (io->master_write_fd, &write_fds);
				if (io->master_write_fd > max_fd)
					max_fd = io->master_write_fd;
			}
		}

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

	if (FD_ISSET (std_io->slave_read_fd, &read_fds))
	{
		/* handle child output */
		n = read_retry (std_io->slave_read_fd,
				std_io->slave_read_buf,
				sizeof std_io->slave_read_buf);
		if (n <= 0)
			return EXIT_FAILURE;

		if (write_loop
		    (std_io->master_write_fd, std_io->slave_read_buf, n) != n)
			error (EXIT_FAILURE, errno, "write");
		total_bytes_written += n;
	}

	if (!child_pid)
		return EXIT_SUCCESS;

	if (std_io->slave_write_fd >= 0 && std_io->master_read_avail
	    && FD_ISSET (std_io->slave_write_fd, &write_fds))
	{
		/* handle child input */
		n = write_loop (std_io->slave_write_fd,
				std_io->master_read_buf,
				std_io->master_read_avail);
		if (n <= 0)
			return EXIT_FAILURE;

		if ((size_t) n < std_io->master_read_avail)
		{
			memmove (std_io->master_read_buf,
				 std_io->master_read_buf + n,
				 std_io->master_read_avail - n);
		}
		total_bytes_read += std_io->master_read_avail;
		std_io->master_read_avail -= n;
	}

	if (std_io->master_read_fd >= 0 && std_io->master_read_avail == 0
	    && FD_ISSET (std_io->master_read_fd, &read_fds))
	{
		/* handle tty input */
		n = read_retry (std_io->master_read_fd,
				std_io->master_read_buf,
				sizeof std_io->master_read_buf);
		if (n > 0)
			std_io->master_read_avail = n;
		else if (n == 0)
		{
			std_io->master_read_buf[0] = 4;
			std_io->master_read_avail = 1;
		} else
			std_io->master_read_fd = -1;
	}

	for (i = 0; i < io_x11_count; ++i)
	{
		io_pair_t io;

		if (!(io = io_x11_list[i]))
			continue;

		if (io->slave_write_fd >= 0 && io->master_read_avail
		    && FD_ISSET (io->slave_write_fd, &write_fds))
		{
			n = write_loop (io->slave_write_fd,
					io->master_read_buf,
					io->master_read_avail);
			if (n <= 0)
			{
				io_x11_free (io);
				continue;
			}

			if ((size_t) n < io->master_read_avail)
			{
				memmove (io->master_read_buf,
					 io->master_read_buf + n,
					 io->master_read_avail - n);
			}
			io->master_read_avail -= n;
		}

		if (io->master_read_fd >= 0 && io->master_read_avail == 0
		    && FD_ISSET (io->master_read_fd, &read_fds))
		{
			n = read_retry (io->master_read_fd,
					io->master_read_buf,
					sizeof io->master_read_buf);
			if (n <= 0)
			{
				io_x11_free (io);
				continue;
			}

			io->master_read_avail = n;
		}

		if (io->master_write_fd >= 0 && io->slave_read_avail
		    && FD_ISSET (io->master_write_fd, &write_fds))
		{
			n = write_loop (io->master_write_fd,
					io->slave_read_buf,
					io->slave_read_avail);
			if (n <= 0)
			{
				io_x11_free (io);
				continue;
			}

			if ((size_t) n < io->slave_read_avail)
			{
				memmove (io->slave_read_buf,
					 io->slave_read_buf + n,
					 io->slave_read_avail - n);
			}
			io->slave_read_avail -= n;
		}

		if (io->slave_read_fd >= 0 && io->slave_read_avail == 0
		    && FD_ISSET (io->slave_read_fd, &read_fds))
		{
			n = read_retry (io->slave_read_fd,
					io->slave_read_buf,
					sizeof io->slave_read_buf);
			if (n <= 0)
			{
				io_x11_free (io);
				continue;
			}

			io->slave_read_avail = n;
		}
	}

	if (x11_fd >= 0 && FD_ISSET (x11_fd, &read_fds))
	{
		int accept_fd, connect_fd;
		if ((accept_fd = x11_accept (x11_fd)) < 0)
		{
			if (x11_accept_timed_out)
			{
				(void) close (x11_fd);
				x11_fd = -1;
			}
		} else
		{
			if ((connect_fd = x11_connect()) >= 0)
				io_x11_new (connect_fd, accept_fd);
			else
				(void) close (accept_fd);
		}
	}
}

int
handle_parent (pid_t child, int a_pty_fd, int pipe_fd, int a_x11_fd)
{
	pty_fd = a_pty_fd;
	x11_fd = a_x11_fd;

	int     in_fd = use_pty ? pty_fd : pipe_fd;
	io_pair_t std_io;

	child_pid = child;

	if (signal (SIGCHLD, sigchld_handler) == SIG_ERR)
		error (EXIT_FAILURE, errno, "signal");

	block_signal_handler (SIGCHLD, SIG_UNBLOCK);
	signal (SIGPIPE, SIG_IGN);

	if (use_pty)
	{
		(void) close (pipe_fd);
		pipe_fd = -1;
	}

	std_io = xmalloc (sizeof (*std_io));
	memset (std_io, 0, sizeof (*std_io));
	std_io->master_read_fd = use_pty ? STDIN_FILENO : -1;
	std_io->master_write_fd = STDOUT_FILENO;
	std_io->slave_read_fd = in_fd;
	std_io->slave_write_fd = use_pty ? in_fd : -1;

	unblock_fd (in_fd);
	if (x11_fd >= 0)
	{
		unblock_fd (x11_fd);
		if (signal (SIGALRM, sigalrm_handler) == SIG_ERR)
			error (EXIT_FAILURE, errno, "signal");
		alarm (5);
	}

	/* redirect standard descriptors, init tty if necessary */
	if (init_tty () && tty_copy_winsize (STDIN_FILENO, pty_fd) == 0)
		(void) signal (SIGWINCH, sigwinch_handler);


	while (work_limits_ok (total_bytes_read, total_bytes_written))
		if (handle_io (std_io) != EXIT_SUCCESS)
			break;

	(void) close (pty_fd);
	wait_child ();
	dfl_signal_handler (SIGCHLD);
	forget_child ();

	return child_rc;
}
