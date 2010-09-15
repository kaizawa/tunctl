#
# Copyright (C) 2010 Kazuyoshi Aizawa. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# $Date: 2007/09/24 10:32:29 $, $Revision: 1.3 $ 

CONFIGURE_FILES = Makefile config.status config.cache config.h config.log

CC = gcc
LD = ld

CFLAGS = -O2 -Wall -I. 

INSTALL = /bin/ginstall -c

exec_prefix = /usr/local
BINDIR = ${exec_prefix}/bin

all: tunctl 

tunctl: tunctl.c if_tun.h
	$(CC) $(CFLAGS) tunctl.c -o tunctl

install: all
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 -o root -g root tunctl $(BINDIR)

uninstall:
	rm $(BINDIR)/tunctl

clean: distclean
	rm -f tunctl *.o *~

distclean:
	rm -f $(CONFIGURE_FILES)
