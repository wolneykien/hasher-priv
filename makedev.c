
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  The makedev action for the hasher-priv program.

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
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "priv.h"

static void
xmknod (const char *name, mode_t mode, dev_t major, dev_t minor)
{
	if (mknod (name, mode, makedev (major, minor)) < 0)
		error (EXIT_FAILURE, errno, "mknod: %s", name);
}

int
do_makedev (void)
{
	mode_t  m;

	chdiruid (chroot_path, CHDIRUID_ABSOLUTE);
	chdiruid ("dev", CHDIRUID_RELATIVE);

	m = umask (0);
	xmknod ("null", S_IFCHR | 0666, 1, 3);
	xmknod ("zero", S_IFCHR | 0666, 1, 5);
	xmknod ("urandom", S_IFCHR | 0644, 1, 9);
	/* I don't want to provide real random. */
	xmknod ("random", S_IFCHR | 0644, 1, 9);
	umask (m);

	return 0;
}
