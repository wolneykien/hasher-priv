
/*
  $Id$
  Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>

  The getugid actions for the pkg-build-priv program.

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

#include <stdio.h>

#include "priv.h"

int
do_getugid1 (void)
{
	printf ("%u:%u\n", change_uid1, change_gid1);
	return 0;
}

int
do_getugid2 (void)
{
	printf ("%u:%u\n", change_uid2, change_gid2);
	return 0;
}
