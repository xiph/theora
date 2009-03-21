INCLUDES = -I$(top_srcdir)/include

noinst_HEADERS = tests.h

AM_CFLAGS = $(OGG_CFLAGS)

THEORADIR = ../lib
THEORA_LIBS = $(THEORADIR)/libtheora.la $(OGG_LIBS)

test: check

TESTS_ENVIRONMENT = $(VALGRIND_ENVIRONMENT)

TESTS = noop comment-test granulepos

noinst_PROGRAMS = $(TESTS)

noop_SOURCES = noop.c
noop_LDADD = $(THEORA_LIBS)
noop_CFLAGS = $(OGG_CFLAGS)

comment_test_SOURCES = comment-test.c
comment_test_LDADD = $(THEORA_LIBS)
comment_test_CFLAGS = $(OGG_CFLAGS)

granulepos_SOURCES = granulepos.c
granulepos_LDADD = $(THEORA_LIBS)
granulepos_CFLAGS = $(OGG_CFLAGS)
