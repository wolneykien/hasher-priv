
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  The caller data initialization module for the pkg-build-priv program.

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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

#include "priv.h"
#include "xmalloc.h"

const char *caller_user, *caller_home;
uid_t   caller_uid;
gid_t   caller_gid;

static const char *
xgetenv (const char *name)
{
	const char *value = getenv (name);

	if (!value || !*value)
		error (EXIT_FAILURE, 0, "undefined variable: %s", name);

	return value;
}

static unsigned long
xatoul (const char *str, const char *name)
{
	char   *p = 0;
	unsigned long n = strtoul (str, &p, 10);

	if (!p || *p || !n || n > INT_MAX)
		error (EXIT_FAILURE, 0, "invalid variable: %s", name);

	return n;
}

void
init_caller_data (void)
{
	struct passwd *pw;

	caller_user = xstrdup (xgetenv ("SUDO_USER"));
	caller_uid = xatoul (xgetenv ("SUDO_UID"), "SUDO_UID");
	caller_gid = xatoul (xgetenv ("SUDO_GID"), "SUDO_GID");

	pw = getpwnam (caller_user);

	if (!pw || !pw->pw_name)
		error (EXIT_FAILURE, 0, "caller %s: lookup failure",
		       caller_user);

	if (strcmp (caller_user, pw->pw_name))
		error (EXIT_FAILURE, 0, "caller %s: name mismatch",
		       caller_user);

	if (caller_uid != pw->pw_uid)
		error (EXIT_FAILURE, 0, "caller %s: uid mismatch",
		       caller_user);

	if (caller_gid != pw->pw_gid)
		error (EXIT_FAILURE, 0, "caller %s: gid mismatch",
		       caller_user);

	if (pw->pw_dir && *pw->pw_dir)
		caller_home = canonicalize_file_name (pw->pw_dir);

	if (!caller_home || !*caller_home)
		error (EXIT_FAILURE, 0, "caller %s: invalid home",
		       caller_user);
}
