## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign 1.6 dist-zip dist-bzip2

SUBDIRS = lib include doc examples tests m4

# we include the whole debian/ dir in EXTRA_DIST because there's a problem
# with autotools and HFS+ MacOSX file systems that caused debian/Makefile.am
# to pick up on the lowercase changelog file and add ChangeLog to DIST_COMMON
# because of it, breaking make dist.  This works just as well.
EXTRA_DIST = \
	COPYING autogen.sh win32 libtheora.spec libtheora.spec.in \
	theora-uninstalled.pc.in \
	debian

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = theora.pc

dist-hook:
	rm -rf `find $(distdir)/debian -name .svn`
	rm -rf `find $(distdir)/debian -name "Makefile*"`
	rm -rf `find $(distdir)/win32 -name .svn`

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
