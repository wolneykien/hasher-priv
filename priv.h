
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  Main include header for the hasher-priv project.

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

#ifndef PKG_BUILD_PRIV_H
#define PKG_BUILD_PRIV_H

#include <sys/types.h>
#include <sys/resource.h>

#define	MIN_CHANGE_UID	34
#define	MIN_CHANGE_GID	34
#define	MAX_CONFIG_SIZE	16384

typedef enum
{
	TASK_NONE = 0,
	TASK_GETUGID1,
	TASK_KILLUID1,
	TASK_CHROOTUID1,
	TASK_GETUGID2,
	TASK_KILLUID2,
	TASK_CHROOTUID2,
	TASK_MAKEDEV
} task_t;

typedef enum
{
	CHDIRUID_RELATIVE = 0,
	CHDIRUID_ABSOLUTE = 1
} chdiruid_t;

typedef struct
{
	const char *name;
	int     resource;
	rlim_t *hard, *soft;
} change_rlimit_t;

void    sanitize_fds (void);
task_t  parse_cmdline (int ac, const char *av[]);
void    init_caller_data (void);
void    configure (void);
void    chdiruid (const char *path, chdiruid_t type);
void    purge_ipc (uid_t uid);

int     do_getugid1 (void);
int     do_killuid1 (void);
int     do_chrootuid1 (void);
int     do_makedev (void);
int     do_getugid2 (void);
int     do_killuid2 (void);
int     do_chrootuid2 (void);

extern const char *__progname;

extern const char *chroot_path;
extern const char **chroot_argv;

extern const char *chroot_prefix;
extern const char *caller_user, *caller_home;
extern uid_t caller_uid;
extern gid_t caller_gid;
extern unsigned caller_num;

extern const char *change_user1, *change_user2;
extern uid_t change_uid1, change_uid2;
extern gid_t change_gid1, change_gid2;
extern mode_t change_umask;
extern int change_nice;
extern change_rlimit_t change_rlimit[];

#endif /* PKG_BUILD_PRIV_H */
