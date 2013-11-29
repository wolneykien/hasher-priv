/*
  Copyright (C) 2011-2012  Dmitry V. Levin <ldv@altlinux.org>

  unshare(2) helpers for the hasher-priv program.

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

/* Code in this file may be executed with root privileges. */

#include <errno.h>
#include <error.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "priv.h"

/*
 * return values:
 *  0 - unshare not requested,
 *  1 - unshare succedded,
 * -1 - unshare failed.
 */
static int
test_unshare(int clone_flags, int share_flag)
{
	if (share_flag > 0)
		return 0;
	if (unshare(clone_flags) == 0)
		return 1;
	if (errno == ENOSYS || errno == EINVAL || errno == EPERM)
		return share_flag ? 0 : -1;
	return -1;
}

int
test_unshare_mount(void)
{
#ifdef CLONE_NEWNS
	return test_unshare(CLONE_NEWNS, share_mount);
#else
# warning "unshare(CLONE_NEWNS) is not available on this system"
	return share_flag ? 0 : -1;
#endif
}

static int
do_unshare(int clone_flags, const char *clone_name,
	   int share_flag, const char *share_name)
{
	if (share_flag > 0)
		return -1;

	if (unshare(clone_flags) < 0)
	{
		if (errno == ENOSYS || errno == EINVAL || errno == EPERM) {
			error(share_flag ? EXIT_SUCCESS : EXIT_FAILURE, errno,
			      "%s isolation is not supported by the kernel",
			      share_name);
			return -1;
		}
		error(EXIT_FAILURE, errno, "unshare %s", clone_name);
	}
	return 0;
}

void
unshare_ipc(void)
{
#ifdef CLONE_NEWIPC
	do_unshare(CLONE_NEWIPC, "CLONE_NEWIPC", share_ipc, "IPC namespace");
#else
# warning "unshare(CLONE_NEWIPC) is not available on this system"
#endif
}

void
unshare_mount(void)
{
#ifdef CLONE_NEWNS
	if (do_unshare(CLONE_NEWNS, "CLONE_NEWNS", share_mount, "mount namespace") < 0)
		return;

	setup_mountpoints();
#else
# warning "unshare(CLONE_NEWNS) is not available on this system"
#endif
}

void
unshare_network(void)
{
#ifdef CLONE_NEWNET
	if (do_unshare(CLONE_NEWNET, "CLONE_NEWNET", share_network, "network") < 0)
		return;

	setup_network();
#else
# warning "unshare(CLONE_NEWNET) is not available on this system"
#endif
}

void
unshare_uts(void)
{
#ifdef CLONE_NEWUTS
	const char *name = "localhost.localdomain";

	if (do_unshare(CLONE_NEWUTS, "CLONE_NEWUTS", share_uts, "UTS namespace") < 0)
		return;

	if (sethostname(name, strlen(name)) < 0)
		error(EXIT_FAILURE, errno, "sethostname: %s", name);
#else
# warning "unshare(CLONE_NEWUTS) is not available on this system"
#endif
}
