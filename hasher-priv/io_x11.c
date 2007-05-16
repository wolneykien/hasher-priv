
/*
  Copyright (C) 2005-2007  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid parent X11 I/O handler for the hasher-priv program.

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

/* Code in this file may be executed with caller privileges. */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "priv.h"
#include "xmalloc.h"

struct io_x11
{
	int     master_fd, slave_fd;
	size_t  master_avail, slave_avail;
	int     authenticated;
	char    master_buf[BUFSIZ], slave_buf[BUFSIZ];
};

typedef struct io_x11 *io_x11_t;

static io_x11_t *io_x11_list;
static size_t io_x11_count;

static  io_x11_t
io_x11_new(int master_fd, int slave_fd)
{
	size_t i;

	for (i = 0; i < io_x11_count; ++i)
		if (!io_x11_list[i])
			break;

	if (i == io_x11_count)
		io_x11_list =
			xrealloc(io_x11_list,
				 ++io_x11_count, sizeof(*io_x11_list));

	io_x11_t io = io_x11_list[i] = xcalloc(1UL, sizeof(*io_x11_list[i]));

	io->master_fd = master_fd;
	io->slave_fd = slave_fd;
	unblock_fd(master_fd);
	unblock_fd(slave_fd);

	return io;
}

static void
io_x11_free(io_x11_t io)
{
	size_t i;

	for (i = 0; i < io_x11_count; ++i)
		if (io_x11_list[i] == io)
			break;
	if (i == io_x11_count)
		error(EXIT_FAILURE, 0,
		      "io_x11_free: entry %p not found, count=%lu\n", io,
		      (unsigned long) io_x11_count);
	io_x11_list[i] = 0;

	(void) close(io->master_fd);
	(void) close(io->slave_fd);
	memset(io, 0, sizeof(*io));
	free(io);
}

void
prepare_x11_new(int *x11_fd, int *max_fd, fd_set * read_fds)
{
	if (*x11_fd < 0)
		return;

	FD_SET(*x11_fd, read_fds);
	if (*x11_fd > *max_fd)
		*max_fd = *x11_fd;
}

void
handle_x11_new(int *x11_fd, fd_set * read_fds)
{
	if (*x11_fd < 0 || !FD_ISSET(*x11_fd, read_fds))
		return;

	int     accept_fd = x11_accept(*x11_fd);

	if (accept_fd < 0)
		return;

	int     connect_fd = x11_connect();

	if (connect_fd >= 0)
		io_x11_new(connect_fd, accept_fd);
	else
		(void) close(accept_fd);
}

void
prepare_x11_select(int *max_fd, fd_set * read_fds, fd_set * write_fds)
{
	size_t i;

	for (i = 0; i < io_x11_count; ++i)
	{
		io_x11_t io;

		if (!(io = io_x11_list[i]))
			continue;

		if (io->slave_avail)
		{
			FD_SET(io->master_fd, write_fds);
			if (io->master_fd > *max_fd)
				*max_fd = io->master_fd;
		} else
		{
			FD_SET(io->slave_fd, read_fds);
			if (io->slave_fd > *max_fd)
				*max_fd = io->slave_fd;
		}

		if (io->master_avail)
		{
			FD_SET(io->slave_fd, write_fds);
			if (io->slave_fd > *max_fd)
				*max_fd = io->slave_fd;
		} else
		{
			FD_SET(io->master_fd, read_fds);
			if (io->master_fd > *max_fd)
				*max_fd = io->master_fd;
		}
	}
}

static void
io_check_auth_data(io_x11_t io, const char *x11_saved_data,
		   const char *x11_fake_data)
{
	if (io->authenticated)
		return;
	io->authenticated = 1;

	unsigned avail = io->slave_avail, expected = 12;

	if (avail < expected)
	{
		error(EXIT_SUCCESS, 0,
		      "Initial X11 packet too short, expected length = %u\r",
		      expected);
		return;
	}
	unsigned proto_len = 0, data_len = 0;
	char   *p = io->slave_buf;

	if (p[0] == 0x42)
	{			/* Byte order MSB first. */
		proto_len = 256 * p[6] + p[7];
		data_len = 256 * p[8] + p[9];
	} else if (p[0] == 0x6c)
	{			/* Byte order LSB first. */
		proto_len = p[6] + 256 * p[7];
		data_len = p[8] + 256 * p[9];
	} else
	{
		error(EXIT_SUCCESS, 0,
		      "Initial X11 packet contains unrecognized order byte: %#x\r",
		      p[0]);
		return;
	}

	expected =
		12 + ((proto_len + 3) & (unsigned) ~3) +
		((data_len + 3) & (unsigned) ~3);
	if (avail < expected)
	{
		error(EXIT_SUCCESS, 0,
		      "Initial X11 packet too short, expected length = %u\r",
		      expected);
		return;
	}

	if (data_len != x11_data_len ||
	    memcmp(p + 12 + ((proto_len + 3) & (unsigned) ~3),
		   x11_fake_data, x11_data_len) != 0)
	{
		error(EXIT_SUCCESS, 0,
		      "X11 auth data does not match fake data\r");
		return;
	}

	memcpy(p + 12 + ((proto_len + 3) & (unsigned) ~3),
	       x11_saved_data, x11_data_len);
}

void
handle_x11_select(fd_set * read_fds, fd_set * write_fds,
		  const char *x11_saved_data, const char *x11_fake_data)
{
	size_t i;
	ssize_t n;

	for (i = 0; i < io_x11_count; ++i)
	{
		io_x11_t io;

		if (!(io = io_x11_list[i]))
			continue;

		if (io->master_avail && FD_ISSET(io->slave_fd, write_fds))
		{
			n = write_loop(io->slave_fd,
				       io->master_buf, io->master_avail);
			if (n <= 0)
			{
				io_x11_free(io);
				continue;
			}

			if ((size_t) n < io->master_avail)
			{
				memmove(io->master_buf,
					io->master_buf + n,
					io->master_avail - n);
			}
			io->master_avail -= n;
		}

		if (!io->master_avail && FD_ISSET(io->master_fd, read_fds))
		{
			n = read_retry(io->master_fd,
				       io->master_buf, sizeof io->master_buf);
			if (n <= 0)
			{
				io_x11_free(io);
				continue;
			}

			io->master_avail = n;
		}

		if (io->slave_avail && FD_ISSET(io->master_fd, write_fds))
		{
			n = write_loop(io->master_fd,
				       io->slave_buf, io->slave_avail);
			if (n <= 0)
			{
				io_x11_free(io);
				continue;
			}

			if ((size_t) n < io->slave_avail)
			{
				memmove(io->slave_buf,
					io->slave_buf + n,
					io->slave_avail - n);
			}
			io->slave_avail -= n;
		}

		if (!io->slave_avail && FD_ISSET(io->slave_fd, read_fds))
		{
			n = read_retry(io->slave_fd,
				       io->slave_buf, sizeof io->slave_buf);
			if (n <= 0)
			{
				io_x11_free(io);
				continue;
			}

			io->slave_avail = n;
			io_check_auth_data(io, x11_saved_data, x11_fake_data);
		}
	}
}
