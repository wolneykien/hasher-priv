
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  Configuration support module for the pkg-build-priv program.

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
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <pwd.h>

#include "priv.h"
#include "xmalloc.h"

const char *chroot_prefix;
const char *change_user1, *change_user2;
uid_t   change_uid1, change_uid2;
gid_t   change_gid1, change_gid2;
mode_t  change_umask = 022;
int     change_nice = 10;
change_limit_t change_limit[] = {

/* Per-process CPU limit, in seconds.  */
	{"cpu", RLIMIT_CPU, 0, 0},

/* Largest file that can be created, in bytes.  */
	{"fsize", RLIMIT_FSIZE, 0, 0},

/* Maximum size of data segment, in bytes.  */
	{"data", RLIMIT_DATA, 0, 0},

/* Maximum size of stack segment, in bytes.  */
	{"stack", RLIMIT_STACK, 0, 0},

/* Largest core file that can be created, in bytes.  */
	{"core", RLIMIT_CORE, 0, 0},

/* Largest resident set size, in bytes.  */
	{"rss", RLIMIT_RSS, 0, 0},

/* Number of processes.  */
	{"nproc", RLIMIT_NPROC, 0, 0},

/* Number of open files.  */
	{"nofile", RLIMIT_NOFILE, 0, 0},

/* Locked-in-memory address space.  */
	{"memlock", RLIMIT_MEMLOCK, 0, 0},

/* Address space limit.  */
	{"as", RLIMIT_AS, 0, 0},

/* Maximum number of file locks.  */
	{"locks", RLIMIT_LOCKS, 0, 0},

/* End of limits.  */
	{0, 0, 0, 0}
};

static void
	__attribute__ ((__noreturn__))
bad_option_name (const char *name, const char *filename)
{
	error (EXIT_FAILURE, 0, "%s: unrecognized option: %s", filename,
	       name);
	exit (EXIT_FAILURE);
}

static void
	__attribute__ ((__noreturn__))
bad_option_value (const char *name, const char *filename)
{
	error (EXIT_FAILURE, 0, "%s: invalid value for \"%s\" option",
	       filename, name);
	exit (EXIT_FAILURE);
}

static  mode_t
str2umask (const char *name, const char *value, const char *filename)
{
	char   *p = 0;
	unsigned long n;

	if (!*value)
		bad_option_value (name, filename);

	n = strtoul (value, &p, 8);
	if (!p || *p || n > 0777)
		bad_option_value (name, filename);

	return n;
}

static unsigned
str2nice (const char *name, const char *value, const char *filename)
{
	char   *p = 0;
	unsigned long n;

	if (!*value)
		bad_option_value (name, filename);

	n = strtoul (value, &p, 10);
	if (!p || *p || n > 19)
		bad_option_value (name, filename);

	return n;
}

static  rlim_t
str2rlim (const char *name, const char *value, const char *filename)
{
	char   *p = 0;
	unsigned long n;

	if (!*value)
		bad_option_value (name, filename);

	if (!strcasecmp (value, "inf"))
		return RLIM_INFINITY;

	n = strtoul (value, &p, 10);
	if (!p || *p || n > INT_MAX)
		bad_option_value (name, filename);

	return n;
}

static void
set_rlim (const char *name, const char *value, int hard,
	  const char *optname, const char *filename)
{
	change_limit_t *p;

	for (p = change_limit; p->name; ++p)
		if (!strcasecmp (name, p->name))
		{
			rlim_t *limit = hard ? p->hard : p->soft;

			free (limit);
			limit = xmalloc (sizeof (*limit));
			*limit = str2rlim (optname, value, filename);
			return;
		}

	bad_option_name (optname, filename);
}

static void
parse_rlim (const char *name, const char *value, const char *optname,
	    const char *filename)
{
	const char hard_prefix[] = "hard_";
	const char soft_prefix[] = "soft_";

	if (!strncasecmp (hard_prefix, name, sizeof (hard_prefix) - 1))
		set_rlim (name + sizeof (hard_prefix) - 1, value, 1,
			  optname, filename);
	else if (!strncasecmp (soft_prefix, name, sizeof (soft_prefix) - 1))
		set_rlim (name + sizeof (soft_prefix) - 1, value, 0,
			  optname, filename);
	else
		bad_option_name (optname, filename);
}

static void
set_config (const char *name, const char *value, const char *filename)
{
	const char rlim_prefix[] = "rlimit_";

	if (!strcasecmp ("user1", name))
		change_user1 = xstrdup (value);
	else if (!strcasecmp ("user2", name))
		change_user2 = xstrdup (value);
	else if (!strcasecmp ("prefix", name))
		chroot_prefix = xstrdup (value);
	else if (!strcasecmp ("umask", name))
		change_umask = str2umask (name, value, filename);
	else if (!strcasecmp ("nice", name))
		change_nice = str2nice (name, value, filename);
	else if (!strncasecmp (rlim_prefix, name, sizeof (rlim_prefix) - 1))
		parse_rlim (name + sizeof (rlim_prefix) - 1, value, name,
			    filename);
	else
		bad_option_name (name, filename);
}

static void
read_config (int fd, const char *name)
{
	FILE   *fp = fdopen (fd, "r");
	char    buf[BUFSIZ];
	unsigned line;

	if (!fp)
		error (EXIT_FAILURE, errno, "fdopen: %s", name);

	for (line = 1; fgets (buf, sizeof buf, fp); ++line)
	{
		const char *start, *left;
		char   *eq, *right, *end;

		for (start = buf; *start && isspace (*start); ++start)
			;

		if (!*start || '#' == *start)
			continue;

		if (!(eq = strchr (start, '=')))
			error (EXIT_FAILURE, 0, "%s: syntax error at line %u",
			       name, line);

		left = start;
		right = eq + 1;

		for (; eq > left; --eq)
			if (!isspace (eq[-1]))
				break;

		if (left == eq)
			error (EXIT_FAILURE, 0, "%s: syntax error at line %u",
			       name, line);

		*eq = '\0';
		end = right + strlen (right);

		for (; right < end; ++right)
			if (!isspace (*right))
				break;

		for (; end > right; --end)
			if (!isspace (end[-1]))
				break;

		*end = '\0';
		set_config (left, right, name);
	}

	if (ferror (fp))
		error (EXIT_FAILURE, errno, "%s", name);
}

static void
check_fd (int fd, const char *name, int regular)
{
	struct stat st;

	if (fstat (fd, &st) < 0)
		error (EXIT_FAILURE, errno, "fstat: %s", name);

	if (st.st_uid)
		error (EXIT_FAILURE, 0, "%s: bad owner: %u", name, st.st_uid);

	if (st.st_mode & (S_IWGRP | S_IWOTH))
		error (EXIT_FAILURE, 0, "%s: bad perms: %o", name,
		       st.st_mode & 07777);

	if (regular)
	{
		if (!S_ISREG (st.st_mode))
			error (EXIT_FAILURE, 0, "%s: bad file type: %o",
			       name, st.st_mode & ~07777);

		if (st.st_size > MAX_CONFIG_SIZE)
			error (EXIT_FAILURE, 0, "%s: file too large: %lu",
			       name, (unsigned long) st.st_size);
	}
}

static void
load_config (const char *name)
{
	int     fd = open (name, O_RDONLY | O_NOFOLLOW | O_NOCTTY);

	if (fd < 0)
		error (EXIT_FAILURE, errno, "open: %s", name);

	check_fd (fd, name, 1);

	read_config (fd, name);

	if (close (fd) < 0)
		error (EXIT_FAILURE, errno, "close: %s", name);
}

/*
 * Change the current working directory.
 * Check ownership, permissions and don't follow symlinks.
 */
static void
xchdir (const char *name)
{
	int     fd = open (name, O_RDONLY | O_NOFOLLOW | O_DIRECTORY);

	if (fd < 0)
		error (EXIT_FAILURE, errno, "open directory: %s", name);

	check_fd (fd, name, 0);

	if (fchdir (fd) < 0)
		error (EXIT_FAILURE, errno, "fchdir: %s", name);

	if (close (fd) < 0)
		error (EXIT_FAILURE, errno, "close: %s", name);
}

static void
check_user (const char *user_name, uid_t * user_uid, gid_t * user_gid,
	    const char *name)
{
	struct passwd *pw;

	if (!user_name || !*user_name)
		error (EXIT_FAILURE, 0, "config: undefined: %s", name);

	pw = getpwnam (user_name);

	if (!pw || !pw->pw_name)
		error (EXIT_FAILURE, 0, "config: %s: %s lookup failure",
		       name, user_name);

	if (strcmp (user_name, pw->pw_name))
		error (EXIT_FAILURE, 0, "config: %s: %s: name mismatch", name,
		       user_name);

	if (pw->pw_uid < MIN_CHANGE_UID)
		error (EXIT_FAILURE, 0, "config: %s: %s: invalid uid: %u",
		       name, user_name, pw->pw_uid);
	*user_uid = pw->pw_uid;

	if (pw->pw_gid < MIN_CHANGE_GID)
		error (EXIT_FAILURE, 0, "config: %s: %s: invalid gid: %u",
		       name, user_name, pw->pw_gid);
	*user_gid = pw->pw_gid;

	if (!strcmp (caller_user, user_name))
		error (EXIT_FAILURE, 0,
		       "config: %s: %s: name coincides with caller", name,
		       user_name);

	if (caller_uid == *user_uid)
		error (EXIT_FAILURE, 0,
		       "config: %s: %s: uid coincides with caller", name,
		       user_name);

	if (caller_gid == *user_gid)
		error (EXIT_FAILURE, 0,
		       "config: %s: %s: gid coincides with caller", name,
		       user_name);
}

void
configure (void)
{
	xchdir ("/");
	xchdir ("etc");
	xchdir ("pkg-build-priv");

	load_config ("system");

	xchdir ("user.d");

	load_config (caller_user);

	if (caller_num)
	{
		char   *fname;

		/* Discard user1 and user2. */
		free ((void *) change_user1);
		change_user1 = 0;

		free ((void *) change_user2);
		change_user2 = 0;

		xasprintf (&fname, "%s:%u", caller_user, caller_num);
		load_config (fname);
		free (fname);
	}

	if (chdir ("/") < 0)
		error (EXIT_FAILURE, errno, "chdir");

	check_user (change_user1, &change_uid1, &change_gid1, "user1");
	check_user (change_user2, &change_uid2, &change_gid2, "user2");

	if (!strcmp (change_user1, change_user2))
		error (EXIT_FAILURE, 0, "config: user1 coincides with user2");

	if (change_uid1 == change_uid2)
		error (EXIT_FAILURE, 0,
		       "config: uid of user1 coincides with uid of user2");

	if (change_gid1 == change_gid2)
		error (EXIT_FAILURE, 0,
		       "config: gid of user1 coincides with gid of user2");
}
