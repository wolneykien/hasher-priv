#
# $Id$
# Copyright (C) 2003, 2004  Dmitry V. Levin <ldv@altlinux.org>
# 
# Makefile for the hasher-priv project.
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

PROJECT = hasher-priv
VERSION = $(shell grep ^Version: hasher-priv.spec |head -1 |awk '{print $$2}')
SCRIPTS = getugid1.sh chrootuid1.sh getugid2.sh chrootuid2.sh makedev.sh
MAN8PAGES = $(PROJECT).8
SUDOERS = $(PROJECT).sudoers
TARGETS = $(PROJECT) $(SCRIPTS) $(SUDOERS) $(MAN8PAGES)

sysconfdir = /etc
libexecdir = /usr/lib
sbindir = /usr/sbin
mandir = /usr/share/man
man8dir = $(mandir)/man8
configdir = $(sysconfdir)/$(PROJECT)
helperdir = $(libexecdir)/$(PROJECT)
DESTDIR =

MKDIR_P = mkdir -p
INSTALL = install
HELP2MAN = help2man -N -s8
RPM_OPT_FLAGS = -pipe -Wall -Werror -O2
CPPFLAGS = $(RPM_OPT_FLAGS) -D_GNU_SOURCE -DENABLE_SETFSUGID -DPROJECT_VERSION=\"$(VERSION)\"

SRC = main.c caller.c chdiruid.c cmdline.c config.c fds.c getugid.c ipc.c killuid.c chrootuid.c makedev.c xmalloc.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

.PHONY:	all install clean indent

all: $(TARGETS)

$(PROJECT): $(OBJ)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

install: all
	$(MKDIR_P) -m710 $(DESTDIR)$(configdir)/user.d
	$(INSTALL) -p -m640 system.conf $(DESTDIR)$(configdir)/system
	$(MKDIR_P) -m750 $(DESTDIR)$(helperdir)
	$(INSTALL) -p -m700 $(PROJECT) $(DESTDIR)$(helperdir)/
	$(INSTALL) -p -m755 $(SCRIPTS) $(DESTDIR)$(helperdir)/
	$(MKDIR_P) -m755 $(DESTDIR)$(sbindir)
	$(INSTALL) -p -m755 hasher-useradd $(DESTDIR)$(sbindir)/
	$(MKDIR_P) -m755 $(DESTDIR)$(man8dir)
	$(INSTALL) -p -m644 $(MAN8PAGES) $(DESTDIR)$(man8dir)/

clean:
	$(RM) $(TARGETS) $(DEP) $(OBJ) core *~

indent:
	indent *.h *.c

%.sh: %.sh.in
	sed -e 's|@helper@|$(helperdir)/$(PROJECT)|g' < $< > $@

%.sudoers: %.sudoers.in
	sed -e 's|@helper@|$(helperdir)/$(PROJECT)|g' < $< > $@

$(PROJECT).8: $(PROJECT)
	$(HELP2MAN) ./$< > $@

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
