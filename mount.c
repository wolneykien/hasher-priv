
/*
  $Id$
  Copyright (C) 2004, 2005  Dmitry V. Levin <ldv@altlinux.org>

  The mount action for the hasher-priv program.

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

/* Code in this file may be executed with root privileges. */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <grp.h>
#include <mntent.h>
#include <sys/mount.h>

#include "priv.h"
#include "xmalloc.h"

static struct mnt_ent
{
	const char *mnt_fsname;
	const char *mnt_dir;
	const char *mnt_type;
	const char *mnt_opts;
} def_fstab[] =
{
	{"proc", "/proc", "proc", "gid=proc"},
	{"devpts", "/dev/pts", "devpts", "noexec,gid=tty,mode=0620"},
	{"sysfs", "/sys", "sysfs", "noexec"}
};

#define def_fstab_size (sizeof (def_fstab) / sizeof (def_fstab[0]))

#ifndef MS_MANDLOCK
#define MS_MANDLOCK	64
#endif
#ifndef MS_DIRSYNC
#define MS_DIRSYNC	128
#endif
#ifndef MS_NOATIME
#define MS_NOATIME	1024
#endif
#ifndef MS_NODIRATIME
#define MS_NODIRATIME	2048
#endif
#ifndef MS_BIND
#define MS_BIND		4096
#endif
#ifndef MS_MOVE
#define MS_MOVE		8192
#endif
#ifndef MS_REC
#define MS_REC		16384
#endif

static struct
{
	const char *name;
	int     invert;
	unsigned long value;
} opt_map[] = {
	{"defaults", 0, 0},
	{"rw", 1, MS_RDONLY},
	{"ro", 0, MS_RDONLY},
	{"suid", 1, MS_NOSUID},
	{"nosuid", 0, MS_NOSUID},
	{"dev", 1, MS_NODEV},
	{"nodev", 0, MS_NODEV},
	{"exec", 1, MS_NOEXEC},
	{"noexec", 0, MS_NOEXEC},
	{"sync", 0, MS_SYNCHRONOUS},
	{"async", 1, MS_SYNCHRONOUS},
	{"mand", 0, MS_MANDLOCK},
	{"nomand", 1, MS_MANDLOCK},
	{"dirsync", 0, MS_DIRSYNC},
	{"dirasync", 1, MS_DIRSYNC},
	{"bind", 0, MS_BIND},
	{"rbind", 0, MS_BIND | MS_REC},
	{"atime", 1, MS_NOATIME},
	{"noatime", 0, MS_NOATIME},
	{"diratime", 1, MS_NODIRATIME},
	{"nodiratime", 0, MS_NODIRATIME}
};

#define opt_map_size (sizeof (opt_map) / sizeof (opt_map[0]))

static void
parse_opt (const char *opt, unsigned long *flags, char **options)
{
	unsigned i;

	for (i = 0; i < opt_map_size; ++i)
		if (!strcmp (opt, opt_map[i].name))
			break;

	if (i < opt_map_size)
	{
		if (opt_map[i].invert)
			*flags &= ~opt_map[i].value;
		else
			*flags |= opt_map[i].value;
		return;
	}

	char   *buf = 0;

	if (!strncmp (opt, "gid=", 4UL) && !isdigit (opt[4]))
	{
		struct group *gr = getgrnam (opt + 4);

		if (gr)
		{
			xasprintf (&buf, "gid=%u", (unsigned) gr->gr_gid);
			opt = buf;
		}
	}

	if (*options)
	{
		*options = xrealloc (*options,
				     strlen (*options) + strlen (opt) + 2);
		strcat (*options, ",");
		strcat (*options, opt);
	} else
	{
		*options = xstrdup (opt);
	}

	free (buf);
}

static void
xmount (struct mnt_ent *e)
{
	if (e->mnt_dir[0] != '/')
		error (EXIT_FAILURE, EINVAL, "xmount: %s", e->mnt_dir);

	char   *options = 0, *opt;
	char   *buf = xstrdup (e->mnt_opts);
	unsigned long flags = MS_MGC_VAL | MS_NOSUID;

	for (opt = strtok (buf, ","); opt; opt = strtok (0, ","))
		parse_opt (opt, &flags, &options);

	chdiruid (chroot_path);
	chdiruid (e->mnt_dir + 1);
	if (mount (e->mnt_fsname, ".", e->mnt_type, flags, options ? : ""))
		error (EXIT_FAILURE, errno, "mount: %s", e->mnt_dir);

	free (options);
	free (buf);
}

static struct mnt_ent **var_fstab;
unsigned var_fstab_size;

static void
load_fstab (void)
{
	const char *name = "fstab";
	struct stat st;
	int     fd = open (name, O_RDONLY | O_NOFOLLOW | O_NOCTTY);

	if (fd < 0)
		error (EXIT_FAILURE, errno, "open: %s", name);

	if (fstat (fd, &st) < 0)
		error (EXIT_FAILURE, errno, "fstat: %s", name);

	stat_rootok_validator (&st, name);

	if (!S_ISREG (st.st_mode))
		error (EXIT_FAILURE, 0, "%s: not a regular file", name);

	if (st.st_size > MAX_CONFIG_SIZE)
		error (EXIT_FAILURE, 0, "%s: file too large: %lu",
		       name, (unsigned long) st.st_size);

	FILE   *fp = fdopen (fd, "r");

	if (!fp)
		error (EXIT_FAILURE, errno, "fdopen: %s", name);

	struct mntent *ent;

	while ((ent = getmntent (fp)))
	{
		struct mnt_ent *e = xmalloc (sizeof (*e));

		e->mnt_fsname = xstrdup (ent->mnt_fsname);
		e->mnt_dir = xstrdup (ent->mnt_dir);
		e->mnt_type = xstrdup (ent->mnt_type);
		e->mnt_opts = xstrdup (ent->mnt_opts);

		var_fstab =
			xrealloc (var_fstab,
				  (var_fstab_size + 1) * sizeof (*var_fstab));
		var_fstab[var_fstab_size++] = e;
	}

	(void) fclose (fp);
}

int
do_mount (void)
{
	char   *targets =
		allowed_mountpoints ? xstrdup (allowed_mountpoints) : 0;
	char   *target = targets ? strtok (targets, " \t,") : 0;

	for (; target; target = strtok (0, " \t,"))
		if (!strcasecmp (target, mountpoint))
			break;

	if (!target)
		error (EXIT_FAILURE, 0,
		       "mount: %s: mount point not allowed", mountpoint);

	safe_chdir ("/", stat_rootok_validator);
	safe_chdir ("etc/hasher-priv", stat_rootok_validator);
	load_fstab ();
	safe_chdir ("/", stat_rootok_validator);

	unsigned i;

	for (i = 0; i < var_fstab_size; ++i)
		if (!strcmp (target, var_fstab[i]->mnt_dir))
			break;

	if (i < var_fstab_size)
		xmount (var_fstab[i]);
	else
	{
		for (i = 0; i < def_fstab_size; ++i)
			if (!strcmp (target, def_fstab[i].mnt_dir))
				break;

		if (i < def_fstab_size)
			xmount (&def_fstab[i]);
		else
			error (EXIT_FAILURE, 0,
			       "mount: %s: mount point not supported",
			       target);
	}

	free (targets);
	return 0;
}
