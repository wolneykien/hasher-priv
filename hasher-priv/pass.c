
/*
  Copyright (C) 2005  Dmitry V. Levin <ldv@altlinux.org>

  The descriptor passing routines for the hasher-priv program.

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

/* Code in this file may be executed with caller or child privileges. */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#include "priv.h"

/* This function may be executed with child privileges. */

void
fd_send(int ctl, int pass, const char *data, size_t data_len)
{
	struct iovec vec;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char    buf[CMSG_SPACE(sizeof pass)];

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof pass);

	*(int *) CMSG_DATA(cmsg) = pass;

	vec.iov_base = (char *) data;
	vec.iov_len = data_len;

	ssize_t rc;

	if ((rc = TEMP_FAILURE_RETRY(sendmsg(ctl, &msg, 0))) !=
	    (ssize_t) data_len)
	{
		if (rc < 0)
		{
			error(EXIT_FAILURE, errno, "sendmsg");
		} else
		{
			if (rc)
				error(EXIT_FAILURE, 0,
				      "sendmsg: expected size %u, got %u",
				      (unsigned) data_len, (unsigned) rc);
			else
				error(EXIT_FAILURE, 0,
				      "sendmsg: unexpected EOF");
		}
	}
}

/* This function may be executed with caller privileges. */

int
fd_recv(int ctl, char *data, size_t data_len)
{
	struct iovec vec;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char    buf[CMSG_SPACE(sizeof(int))];

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;

	vec.iov_base = data;
	vec.iov_len = data_len;

	ssize_t rc;

	if ((rc = TEMP_FAILURE_RETRY(recvmsg(ctl, &msg, 0))) !=
	    (ssize_t) data_len)
	{
		if (rc < 0)
		{
			error(EXIT_SUCCESS, errno, "recvmsg");
			fputc('\r', stderr);
		} else
		{
			if (rc)
				error(EXIT_SUCCESS, 0,
				      "recvmsg: expected size %u, got %u\r",
				      (unsigned) data_len, (unsigned) rc);
			else
				error(EXIT_SUCCESS, 0,
				      "recvmsg: unexpected EOF\r");
		}
		return -1;
	}

	if (!(cmsg = CMSG_FIRSTHDR(&msg)))
	{
		error(EXIT_SUCCESS, 0, "recvmsg: no message header\r");
		return -1;
	}

	if (cmsg->cmsg_type != SCM_RIGHTS)
	{
		error(EXIT_SUCCESS, 0, "recvmsg: expected type %u, got %u\r",
		      SCM_RIGHTS, cmsg->cmsg_type);
		return -1;
	}

	return *(int *) CMSG_DATA(cmsg);
}
