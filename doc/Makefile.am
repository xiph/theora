## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)

built_docs = spec.py spec.html

static_docs = vp3-format.txt
#		testspec.ogg testspec.raw \
#		testspec2.ogg testspec2.raw

doc_DATA = $(built_docs) $(static_docs)

EXTRA_DIST = $(built_docs) $(static_docs) spec.txt \
	     txt2py.py txt2html.py

CLEANFILES = $(built_docs)

spec.py : spec.txt txt2py.py
	python $(srcdir)/txt2py.py $(srcdir)/spec.txt spec.py

spec.html : spec.txt txt2html.py
	python $(srcdir)/txt2html.py $(srcdir)/spec.txt spec.html

