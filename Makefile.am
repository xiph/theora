## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign 1.6 dist-zip dist-bzip2

SUBDIRS = lib include doc examples debian

EXTRA_DIST = COPYING autogen.sh win32 libtheora.spec libtheora.spec.in \
	theora-uninstalled.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = theora.pc

dist-hook:
	rm -rf `find $(distdir)/macos -name CVS`
	rm -rf `find $(distdir)/win32 -name CVS`
	rm -rf `find $(distdir)/macos -name .svn`
	rm -rf `find $(distdir)/win32 -name .svn`
	rm -rf `find $(distdir)/doc -name .svn`

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
