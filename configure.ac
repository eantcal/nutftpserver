#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([nuTftpServer], [1.0], [antonino.calderone@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([nuAssert.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC


# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdio_ext.h stdlib.h string.h sys/ioctl.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([socket bind send recv])

# ---------------------------------------------------------------------------- #

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
