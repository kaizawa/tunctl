# $Date: 2007/09/24 10:32:29 $, $Revision: 1.3 $ 
# Kazuyoshi <admin2@whiteboard.ne.jp>

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
