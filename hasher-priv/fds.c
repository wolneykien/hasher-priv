
/*
  Copyright (C) 2003-2008  Dmitry V. Levin <ldv@altlinux.org>

  The file descriptor sanitizer for the hasher-priv program.

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

/* Code in this file may be executed with root or caller privileges. */

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <limits.h>

#include "priv.h"

/* This function may be executed with root privileges. */
static int
get_open_max(void)
{
	long int i;

	i = sysconf(_SC_OPEN_MAX);
	if (i < NR_OPEN)
		i = NR_OPEN;
	if (i > INT_MAX)
		i = INT_MAX;

	return (int) i;
}


/* This function may be executed with root privileges. */
void
sanitize_fds(void)
{
	int     fd, max_fd;

	/* Set safe umask, just in case. */
	umask(077);

	/* Check for stdin, stdout and stderr: they should present. */
	for (fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd)
	{
		struct stat st;

		if (fstat(fd, &st) < 0)
			/* At this stage, we shouldn't even report error. */
			exit(EXIT_FAILURE);
	}

	max_fd = get_open_max();

	/* Close all the rest. */
	for (; fd < max_fd; ++fd)
		(void) close(fd);

	errno = 0;
}

/* This function may be executed with root privileges. */
void
cloexec_fds(void)
{
	int     fd, max_fd = get_open_max();

	/* Set close-on-exec flag on all non-standard descriptors. */
	for (fd = STDERR_FILENO + 1; fd < max_fd; ++fd)
	{
		int     flags = fcntl(fd, F_GETFD, 0);

		if (flags < 0)
			continue;

		int     newflags = flags | FD_CLOEXEC;

		if (flags != newflags && fcntl(fd, F_SETFD, newflags))
			error(EXIT_FAILURE, errno, "fcntl F_SETFD");
	}

	errno = 0;
}

/* This function may be executed with caller or child privileges. */
void
nullify_stdin(void)
{
	int     fd = open(_PATH_DEVNULL, O_RDONLY);

	if (fd < 0)
		error(EXIT_FAILURE, errno, "open: %s", _PATH_DEVNULL);
	if (fd != STDIN_FILENO)
	{
		if (dup2(fd, STDIN_FILENO) != STDIN_FILENO)
			error(EXIT_FAILURE, errno, "dup2(%d, %d)",
			      fd, STDIN_FILENO);
		if (close(fd) < 0)
			error(EXIT_FAILURE, errno, "close(%d)", fd);
	}
}

/* This function may be executed with caller privileges. */
void
unblock_fd(int fd)
{
	int     flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
		error(EXIT_FAILURE, errno, "fcntl F_GETFL");

	int     newflags = flags | O_NONBLOCK;

	if (flags != newflags && fcntl(fd, F_SETFL, newflags) < 0)
		error(EXIT_FAILURE, errno, "fcntl F_SETFL");
}

/* This function may be executed with caller privileges. */
ssize_t
read_retry(int fd, void *buf, size_t count)
{
	return TEMP_FAILURE_RETRY(read(fd, buf, count));
}

/* This function may be executed with caller privileges. */
ssize_t
write_retry(int fd, const void *buf, size_t count)
{
	return TEMP_FAILURE_RETRY(write(fd, buf, count));
}

/* This function may be executed with caller privileges. */
ssize_t
write_loop(int fd, const char *buffer, size_t count)
{
	ssize_t offset = 0;

	while (count > 0)
	{
		ssize_t block = write_retry(fd, &buffer[offset], count);

		if (block <= 0)
			return offset ? : block;
		offset += block;
		count -= (size_t) block;
	}
	return offset;
}

/* This function may be executed with caller privileges. */
void
fds_add_fd(fd_set *fds, int *max_fd, const int fd)
{
	if (fd < 0)
		return;

	FD_SET(fd, fds);
	if (fd > *max_fd)
		*max_fd = fd;
}

/* This function may be executed with caller privileges. */
int
fds_isset(fd_set *fds, const int fd)
{
	return (fd >= 0) && FD_ISSET(fd, fds);
}
