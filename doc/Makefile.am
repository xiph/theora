## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = python

docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)

built_docs =

static_docs = vp3-format.txt color.html

doc_DATA = $(built_docs) $(static_docs)

EXTRA_DIST = $(built_docs) $(static_docs) spec

CLEANFILES = $(built_docs)

