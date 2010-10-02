#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(embed_python, 0.1)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([embed_python.cc])
dnl AC_CONFIG_HEADER([config.h])

AC_PROG_CXX

AC_PROG_LIBTOOL

AC_CHECK_HEADERS([libecap/adapter/service.h])
AC_CHECK_LIB([ecap], [main],
    [LIBS="-lecap $LIBS"],
    [AC_MSG_FAILURE([libecap is not found or unusable])]
)

# ToDo: Maybe there is a standard autoconf test for Python stuff.
python_cflags=`python-config --cflags`
python_ldflags=`python-config --ldflags`
AC_SUBST(python_cflags)
AC_SUBST(python_ldflags)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
