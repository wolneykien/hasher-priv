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
SCRIPTS = getugid1.sh chrootuid1.sh getugid2.sh chrootuid2.sh makedev.sh maketty.sh
MAN5PAGES = $(PROJECT).conf.5
MAN8PAGES = $(PROJECT).8 hasher-useradd.8
SUDOERS = $(PROJECT).sudoers
TARGETS = $(PROJECT) $(SCRIPTS) $(SUDOERS) $(MAN5PAGES) $(MAN8PAGES)

sysconfdir = /etc
libexecdir = /usr/lib
sbindir = /usr/sbin
mandir = /usr/share/man
man5dir = $(mandir)/man5
man8dir = $(mandir)/man8
configdir = $(sysconfdir)/$(PROJECT)
helperdir = $(libexecdir)/$(PROJECT)
DESTDIR =

MKDIR_P = mkdir -p
INSTALL = install
HELP2MAN8 = help2man -N -s8
CPPFLAGS = -D_GNU_SOURCE -DENABLE_SETFSUGID -DENABLE_SUPPLEMENTARY_GROUPS -D_FILE_OFFSET_BITS=64 -DPROJECT_VERSION=\"$(VERSION)\"
CFLAGS = -pipe -Wall -Werror -W -O2
LDLIBS = -lutil

SRC = caller.c chdir.c chdiruid.c chrootuid.c cmdline.c config.c fds.c \
	getugid.c ipc.c killuid.c main.c makedev.c mount.c parent.c \
	signal.c tty.c umount.c xmalloc.c
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
	$(MKDIR_P) -m755 $(DESTDIR)$(man5dir)
	$(INSTALL) -p -m644 $(MAN5PAGES) $(DESTDIR)$(man5dir)/
	$(MKDIR_P) -m755 $(DESTDIR)$(man8dir)
	$(INSTALL) -p -m644 $(MAN8PAGES) $(DESTDIR)$(man8dir)/

clean:
	$(RM) $(TARGETS) $(DEP) $(OBJ) core *~

indent:
	indent *.h *.c

%.sh: %.sh.in Makefile
	sed -e 's|@helper@|$(helperdir)/$(PROJECT)|g' <$< >$@

%.sudoers: %.sudoers.in Makefile
	sed -e 's|@helper@|$(helperdir)/$(PROJECT)|g' <$< >$@

%.5: %.5.in
	sed -e 's/@VERSION@/$(VERSION)/g' <$< >$@
	chmod --reference=$< $@

%.8: % %.8.inc Makefile
	$(HELP2MAN8) -i $@.inc ./$< >$@

# We need dependencies only if goal isn't "indent" or "clean".
ifneq ($(MAKECMDGOALS),indent)
ifneq ($(MAKECMDGOALS),clean)

%.d:	%.c Makefile
	@echo Making dependences for $<
	@$(CC) -MM $(CPPFLAGS) $< |sed -e 's,\($*\)\.o[ :]*,\1.o $@: Makefile ,g' >$@

ifneq ($(DEP),)
-include $(DEP)
endif

endif # clean
endif # indent
