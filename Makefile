#
# $Id$
# Copyright (C) 2003  Dmitry V. Levin <ldv@altlinux.org>
# 
# Makefile for the pkg-build-priv project.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

PROJECT = pkg-build-priv
SCRIPTS = getugid1.sh chrootuid1.sh getugid2.sh chrootuid2.sh makedev.sh
TARGETS = $(PROJECT) $(SCRIPTS)

sysconfdir = /etc
libexecdir = /usr/lib
configdir = $(sysconfdir)/$(PROJECT)
helperdir = $(libexecdir)/$(PROJECT)
DESTDIR =

MKDIR_P = mkdir -p
INSTALL = install
CPPFLAGS = $(RPM_OPT_FLAGS) -Wall -D_GNU_SOURCE -DENABLE_SETFSUGID

SRC = main.c caller.c chdiruid.c cmdline.c config.c fds.c getugid.c killuid.c chrootuid.c makedev.c xmalloc.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

.PHONY:	all install clean indent

all: $(TARGETS)

$(PROJECT): $(OBJ)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

install: all
	$(MKDIR_P) -m700 $(DESTDIR)$(configdir)
	$(MKDIR_P) -m700 $(DESTDIR)$(configdir)/user.d
	$(INSTALL) -p -m600 /dev/null $(DESTDIR)$(configdir)/system
	$(MKDIR_P) -m750 $(DESTDIR)$(helperdir)
	$(INSTALL) -p -m700 $(PROJECT) $(DESTDIR)$(helperdir)/
	$(INSTALL) -p -m755 $(SCRIPTS) $(DESTDIR)$(helperdir)/

clean:
	$(RM) $(TARGETS) $(DEP) $(OBJ) core *~

indent:
	indent *.h *.c

%.sh:	%.sh.in
	sed -e 's|@helper@|$(helperdir)/$(PROJECT)|g' < $< > $@

# We need dependencies only if goal isn't "indent" or "clean".
ifneq ($(MAKECMDGOALS),indent)
ifneq ($(MAKECMDGOALS),clean)

%.d:	%.c
	@echo Making dependences for $<
	@$(SHELL) -ec "$(CC) -MM $(CPPFLAGS) $< | sed -e 's|\($*\)\.o[ :]*|\1.o $@ : |g' > $@; [ -s $@ ] || $(RM) $@"

ifneq ($(DEP),)
-include $(DEP)
endif

endif # clean
endif # indent
