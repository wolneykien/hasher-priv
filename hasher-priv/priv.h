
/*
  Copyright (C) 2003-2008  Dmitry V. Levin <ldv@altlinux.org>

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef PKG_BUILD_PRIV_H
#define PKG_BUILD_PRIV_H

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define	MIN_CHANGE_UID	34
#define	MIN_CHANGE_GID	34
#define	MAX_CONFIG_SIZE	16384

typedef enum
{
	TASK_NONE = 0,
	TASK_GETCONF,
	TASK_GETUGID1,
	TASK_KILLUID1,
	TASK_CHROOTUID1,
	TASK_GETUGID2,
	TASK_KILLUID2,
	TASK_CHROOTUID2,
	TASK_MAKEDEV,
	TASK_MAKETTY,
	TASK_MAKECONSOLE,
	TASK_MOUNT,
	TASK_UMOUNT
} task_t;

typedef struct
{
	const char *name;
	int     resource;
	rlim_t *hard, *soft;
} change_rlimit_t;

typedef struct
{
	unsigned time_elapsed;
	unsigned time_idle;
	unsigned bytes_read;
	unsigned bytes_written;
} work_limit_t;

typedef void VALIDATE_FPTR(struct stat *, const char *);

void    sanitize_fds(void);
void    cloexec_fds(void);
void    nullify_stdin(void);
void    unblock_fd(int fd);
ssize_t read_retry(int fd, void *buf, size_t count);
ssize_t write_retry(int fd, const void *buf, size_t count);
ssize_t write_loop(int fd, const char *buffer, size_t count);
int     init_tty(void);
void    restore_tty(void);
int     tty_copy_winsize(int master_fd, int slave_fd);
task_t  parse_cmdline(int ac, const char *av[]);
void    init_caller_data(void);
void    parse_env(void);
void    configure(void);
void    ch_uid(uid_t uid, uid_t *save);
void    ch_gid(gid_t gid, gid_t *save);
void    chdiruid(const char *path);
void    purge_ipc(uid_t uid);
int     handle_child(char *const *env, int pty_fd, int pipe_out, int pipe_err, int ctl_fd);
int     handle_parent(pid_t pid, int pty_fd, int pipe_out, int pipe_err, int ctl_fd);
void    block_signal_handler(int no, int what);
void    dfl_signal_handler(int no);
void    safe_chdir(const char *name, VALIDATE_FPTR validator);
void    stat_userok_validator(struct stat *st, const char *name);
void    stat_rootok_validator(struct stat *st, const char *name);
void    stat_anyok_validator(struct stat *st, const char *name);
void    fd_send(int ctl, int pass, const char *data, size_t len);
int     fd_recv(int ctl, char *data, size_t data_len);
int     unix_accept(int fd);
int     log_listen(void);
int     x11_parse_display(void);
void    x11_closedir(void);
int     x11_listen(void);
int     x11_connect(void);
int     x11_check_listen(int fd);
void    prepare_x11_new(int *x11_fd, int *max_fd, fd_set * read_fds);
void    handle_x11_new(int *x11_fd, fd_set * read_fds);
void    prepare_x11_select(int *max_fd, fd_set * read_fds,
			   fd_set * write_fds);
void    handle_x11_select(fd_set * read_fds, fd_set * write_fds,
			  const char *x11_saved_data,
			  const char *x11_fake_data);

int     do_getconf(void);
int     do_getugid1(void);
int     do_killuid1(void);
int     do_chrootuid1(void);
int     do_getugid2(void);
int     do_killuid2(void);
int     do_chrootuid2(void);
int     do_makeconsole(void);
int     do_makedev(void);
int     do_maketty(void);
int     do_mount(void);
int     do_umount(void);

extern const char *chroot_path;
extern const char **chroot_argv;

extern const char *mountpoint;
extern const char *allowed_mountpoints;

extern const char *term;
extern const char *x11_display, *x11_key;

extern int allow_tty_devices, use_pty;
extern size_t x11_data_len;

extern const char *const *chroot_prefix_list;
extern const char *chroot_prefix_path;
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
extern work_limit_t wlimit;

#endif /* PKG_BUILD_PRIV_H */
