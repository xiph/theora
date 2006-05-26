## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign 1.6 dist-zip dist-bzip2

SUBDIRS = lib include doc examples tests m4

# we include the whole debian/ dir in EXTRA_DIST because there's a problem
# with autotools and HFS+ MacOSX file systems that caused debian/Makefile.am
# to pick up on the lowercase changelog file and add ChangeLog to DIST_COMMON
# because of it, breaking make dist.  This works just as well.
EXTRA_DIST = \
	README CHANGES COPYING LICENSE \
	autogen.sh win32 symbian SConstruct \
	libtheora.spec libtheora.spec.in \
	theora-uninstalled.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = theora.pc

dist-hook:
	for item in $(EXTRA_DIST); do \
	  if test -d $$item; then \
	    echo -n "cleaning $$item dir for distribution..."; \
	    rm -rf `find $(distdir)/$$item -name .svn`; \
	    echo "OK"; \
	  fi; \
	done

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
