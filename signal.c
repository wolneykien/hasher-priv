
/*
  $Id$
  Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>

  The chrootuid signal handling for the hasher-priv program.

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

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "priv.h"

/* This function may be executed with root privileges. */
void
block_signal_handler (int no, int what)
{
	sigset_t set;

	sigemptyset (&set);
	sigaddset (&set, no);
	if (sigprocmask (what, &set, 0) < 0)
		error (EXIT_FAILURE, errno, "sigprocmask");
}

/* This function may be executed with caller or child privileges. */
void
dfl_signal_handler (int no)
{
	if (signal (no, SIG_DFL) == SIG_ERR)
		error (EXIT_FAILURE, errno, "signal");

	block_signal_handler (no, SIG_UNBLOCK);
}
