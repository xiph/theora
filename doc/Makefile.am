## Process this file with automake to produce Makefile.in

SUBDIRS = #python

docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)

built_docs =

static_docs = vp3-format.txt color.html

doc_DATA = $(built_docs) $(static_docs) doxygen-build.stamp

EXTRA_DIST = $(built_docs) $(static_docs) spec Doxyfile.in

CLEANFILES = $(built_docs)

if HAVE_DOXYGEN
doxygen-build.stamp: Doxyfile $(top_srcdir)/include/theora/*.h
	doxygen
	touch doxygen-build.stamp
else
doxygen-build.stamp:
	echo "*** Warning: Doxygen not found; documentation will not be built."
	touch doxygen-build.stamp
endif

dist_docdir = $(distdir)/libtheora

dist-hook:
	if test -d libtheora; then \
	  mkdir $(dist_docdir); \
	  for dir in libtheora/*; do \
	    b=`basename $$dir`; \
	    if test $$b != "CVS"; then \
	      if test -d $$dir; then \
		mkdir $(dist_docdir)/$$b; \
		for f in $$dir/*; do \
		  cp -p $$f $(dist_docdir)/$$b; \
		done \
	      fi \
	    fi \
	  done \
	fi


install-data-local: doxygen-build.stamp
	$(mkinstalldirs) $(docdir)
	if test -d libtheora; then \
	  for dir in libtheora/*; do \
	    if test -d $$dir; then \
	      b=`basename $$dir`; \
	      $(mkinstalldirs) $(docdir)/$$b; \
	      for f in $$dir/*; do \
		$(INSTALL_DATA) $$f $(docdir)/$$b; \
	      done \
	    fi \
	  done \
	fi

uninstall-local:
	rm -rf $(docdir)

clean-local:
	if test -d libtheora; then rm -rf libtheora; fi
	if test -f doxygen-build.stamp; then rm -f doxygen-build.stamp; fi

