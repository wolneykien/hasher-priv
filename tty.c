
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid tty functions for the hasher-priv program.

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
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "priv.h"

static int tty_is_saved;
static struct termios tty_orig;

void
restore_tty (void)
{
	if (tty_is_saved)
	{
		/* restore only once */
		tty_is_saved = 0;
		tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_orig);
	}
}

int
init_tty (void)
{
	if (tcgetattr (STDIN_FILENO, &tty_orig))
		return 0;	/* not a tty */

	if (use_pty)
	{
		struct termios tty_changed = tty_orig;

		tty_is_saved = 1;
		if (atexit (restore_tty))
			error (EXIT_FAILURE, errno, "atexit");

		cfmakeraw (&tty_changed);
		tty_changed.c_iflag |= IXON;
		tty_changed.c_cc[VMIN] = 1;
		tty_changed.c_cc[VTIME] = 0;

		if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_changed))
			error (EXIT_FAILURE, errno, "tcsetattr");

		return 1;
	} else
	{
		nullify_stdin ();
		return 0;
	}
}

int
tty_copy_winsize (int master_fd, int slave_fd)
{
	int     rc;
	struct winsize ws;

	if ((rc = ioctl (master_fd, TIOCGWINSZ, &ws)) < 0)
		return rc;
	return ioctl (slave_fd, TIOCSWINSZ, &ws);
}
