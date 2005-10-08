
/*
  $Id$
  Copyright (C) 2002, 2005  Dmitry V. Levin <ldv@altlinux.org>

  Dynamic memory allocation with error checking.

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

#ifndef __XMALLOC_H__
#define __XMALLOC_H__

extern void *xmalloc(size_t size);
extern void *xrealloc(void *ptr, size_t size);
extern char *xstrdup(const char *s);
extern char *xasprintf(char **ptr, const char *fmt, ...)
	__attribute__ ((__format__(__printf__, 2, 3)));

#endif /* __XMALLOC_H__ */
