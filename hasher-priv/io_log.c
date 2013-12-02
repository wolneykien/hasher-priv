
/*
  Copyright (C) 2008  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid parent log I/O handler for the hasher-priv program.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Code in this file may be executed with caller privileges. */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "priv.h"
#include "xmalloc.h"

static int *fd_list;
static size_t fd_count;

static void
fd_new(int fd)
{
	if (fd < 0)
		return;

	size_t  i;

	for (i = 0; i < fd_count; ++i)
		if (fd_list[i] < 0)
			break;

	if (i == fd_count)
		fd_list = xrealloc(fd_list, ++fd_count, sizeof(*fd_list));

	fd_list[i] = fd;
	unblock_fd(fd);
}

static void
fd_free(const int fd)
{
	size_t  i;

	for (i = 0; i < fd_count; ++i)
		if (fd == fd_list[i])
			break;

	if (i == fd_count)
		error(EXIT_FAILURE, 0,
		      "fd_free: descriptor %d not found, count=%lu\n",
		      fd, (unsigned long) fd_count);

	fd_list[i] = -1;
	(void) close(fd);
}

void
log_handle_new(const int fd, fd_set *fds)
{
	if (fds_isset(fds, fd))
		fd_new(unix_accept(fd));
}

void
fds_add_log(fd_set *fds, int *max_fd)
{
	size_t  i;

	for (i = 0; i < fd_count; ++i)
		fds_add_fd(fds, max_fd, fd_list[i]);
}

static void
copy_log(const int fd)
{
	ssize_t i;
	char    buf[BUFSIZ];

	i = read_retry(fd, buf, sizeof(buf) - 2);
	if (i <= 0)
	{
		fd_free(fd);
		return;
	}

	buf[i] = '\0';

	size_t  n = strlen(buf);

	if (n > 0 && buf[n - 1] != '\n')
	{
		buf[n++] = '\r';
		buf[n++] = '\n';
	}

	xwrite_all(STDERR_FILENO, buf, n);
}


void
log_handle_select(fd_set *fds)
{
	size_t  i;

	for (i = 0; i < fd_count; ++i)
		if (fds_isset(fds, fd_list[i]))
			copy_log(fd_list[i]);
}
