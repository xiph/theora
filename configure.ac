dnl Process this file with autoconf to produce a configure script

dnl ------------------------------------------------
dnl Initialization and Versioning
dnl ------------------------------------------------

AC_INIT(libtheora,[unreleased])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([lib/dct.c])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

dnl Library versioning

V_LIB_CURRENT=0
V_LIB_REVISION=0
V_LIB_AGE=0
AC_SUBST(V_LIB_CURRENT)
AC_SUBST(V_LIB_REVISION)
AC_SUBST(V_LIB_AGE)

dnl --------------------------------------------------  
dnl Check for programs
dnl --------------------------------------------------  

dnl save $CFLAGS since AC_PROG_CC likes to insert "-g -O2"
dnl if $CFLAGS is blank
cflags_save="$CFLAGS"
AC_PROG_CC
AC_PROG_CPP
CFLAGS="$cflags_save"

AM_PROG_LIBTOOL

dnl Check for doxygen
AC_CHECK_PROG(HAVE_DOXYGEN, doxygen, true, false)
AM_CONDITIONAL(HAVE_DOXYGEN,$HAVE_DOXYGEN)
if test $HAVE_DOXYGEN = "false"; then
        AC_MSG_WARN([*** doxygen not found, API documentation will not be built])
fi

dnl --------------------------------------------------
dnl Set build flags based on environment
dnl --------------------------------------------------

dnl Set some target options

cflags_save="$CFLAGS"
ldflags_save="$LDFLAGS"
if test -z "$GCC"; then
        case $host in 
        *)
                DEBUG="-g"
                CFLAGS="-O"
                PROFILE="-g -p" ;;
        esac
else

        case $host in 
        *)
                DEBUG="-g -W -D__NO_MATH_INLINES"
                CFLAGS="-O2 -Wall"
                PROFILE="-W -pg -g -O2 -fno-inline-functions";;
        esac
fi
CFLAGS="$CFLAGS $cflags_save"
LDFLAGS="$LDFLAGS $ldflags_save"

dnl --------------------------------------------------
dnl Overall build configuration options
dnl --------------------------------------------------

dnl Configuration option for building of floating point code.

ac_enable_float=yes
AC_ARG_ENABLE(float,
     [  --disable-float         disable use of floating point code ],
     [ ac_enable_float=no ], [ ac_enable_float=yes] )

if test "x${ac_enable_float}" = xyes ; then
    AC_DEFINE(THEORA_SUPPORT_FLOAT, [1], [Build floating point code])
else
    AC_DEFINE(THEORA_SUPPORT_FLOAT, [0], [Do not build floating point code])
fi
AM_CONDITIONAL(THEORA_SUPPORT_FLOAT, [test "x${ac_enable_float}" = "xyes"])

dnl Configuration option for building of encoding support.

ac_enable_encode=yes
AC_ARG_ENABLE(encode,
     [  --disable-encode        disable encoding support ],
     [ ac_enable_encode=no ], [ ac_enable_encode=yes] )

if test "x${ac_enable_encode}" = xyes ; then
    BUILDABLE_EXAMPLES="$BUILDABLE_EXAMPLES encoder_example"
    AC_DEFINE(THEORA_SUPPORT_ENCODE, [1], [Build encoding support])
else
    AC_DEFINE(THEORA_SUPPORT_ENCODE, [0], [Do not build encoding support])
fi
AM_CONDITIONAL(THEORA_SUPPORT_ENCODE, [test "x${ac_enable_encode}" = "xyes"])

dnl --------------------------------------------------
dnl Check for headers
dnl --------------------------------------------------

dnl none here

dnl --------------------------------------------------
dnl Check for typedefs, structures, etc
dnl --------------------------------------------------

dnl none

dnl --------------------------------------------------
dnl Check for library functions
dnl --------------------------------------------------

dnl substitue the included getopt if the system doesn't support long options
AC_CHECK_FUNC(getopt_long, [GETOPT_OBJS=''], [GETOPT_OBJS='getopt.$(OBJEXT) getopt1.$(OBJEXT)'])
AC_SUBST(GETOPT_OBJS)

XIPH_PATH_OGG(, AC_MSG_ERROR([
    libogg is required to build this package!
    please see http://www.xiph.org/ for how to
    obtain a copy.
]))
CFLAGS="$CFLAGS $OGG_CFLAGS"
LIBS="$LIBS $OGG_LIBS"
AC_CHECK_FUNC(oggpackB_read, , [
  AC_MSG_ERROR([newer libogg version (>1.0) required])
])

AM_PATH_SDL(,[
	HAVE_SDL=yes
	SDL_LIBS=`$SDL_CONFIG --libs`
],AC_MSG_WARN([*** Unable to find SDL -- Not compiling example players ***]))

AC_CHECK_HEADERS([sys/soundcard.h soundcard.h machine/soundcard.h],[
  HAVE_OSS=yes
  break
],AC_MSG_WARN([OSS audio support not found -- not compiling player_example]))

if test x$HAVE_SDL = xyes -a x$HAVE_OSS = xyes; then
  BUILDABLE_EXAMPLES="$BUILDABLE_EXAMPLES player_example"
fi
AC_SUBST(BUILDABLE_EXAMPLES)

dnl --------------------------------------------------
dnl Do substitutions
dnl --------------------------------------------------

LIBS="$LIBS"

AC_SUBST(LIBS)
AC_SUBST(DEBUG)
AC_SUBST(PROFILE)

AC_OUTPUT([
  Makefile lib/Makefile
  include/Makefile include/theora/Makefile
  examples/Makefile
  doc/Makefile doc/Doxyfile doc/python/Makefile
  debian/Makefile
  theora.pc
])
