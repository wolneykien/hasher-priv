
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <errno.h>
#include <error.h>
#include <paths.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>

#include "priv.h"

void
sanitize_fds (void)
{
	int     fd, max_fd;

	/* Just in case. */
	umask (077);

	/* Check for stdin, stdout and stderr: they should present. */
	for (fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd)
	{
		struct stat st;

		if (fstat (fd, &st) < 0)
			/* At this stage, we shouldn't even report error. */
			exit (EXIT_FAILURE);
	}

	max_fd = sysconf (_SC_OPEN_MAX);
	if (max_fd < NR_OPEN)
		max_fd = NR_OPEN;

	/* Close all the rest. */
	for (; fd < max_fd; ++fd)
		close (fd);

	/* If stdin is a tty, reopen it with /dev/null. */
	if (isatty (STDIN_FILENO))
	{
		fd = open (_PATH_DEVNULL, O_RDONLY);
		if (fd < 0)
			error (EXIT_FAILURE, errno, "open: %s",
			       _PATH_DEVNULL);
		if (fd != STDIN_FILENO)
		{
			if (dup2 (fd, STDIN_FILENO) != STDIN_FILENO)
				error (EXIT_FAILURE, errno, "dup2");
			if (fd > STDERR_FILENO && close (fd) < 0)
				error (EXIT_FAILURE, errno, "close");
		}
	}

	errno = 0;
}
