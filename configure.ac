# GtkTerm autoconf script
AC_PREREQ([2.64])
AC_INIT([GtkTerm],[1.0],[https://github.com/phako/gtkterm])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/gtkterm.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 tar-ustar no-dist-gzip dist-xz foreign -Wall -Wno-portability])
AM_MAINTAINER_MODE([enable])

AX_GENERATE_CHANGELOG

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Compiler flags
AX_IS_RELEASE([git-directory])
AX_CHECK_ENABLE_DEBUG
AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

AC_PROG_CC

# Linux is required for arbitrary baud rates
AC_CHECK_HEADERS_ONCE([linux/serial.h])

# Dependencies
PKG_CHECK_MODULES([gtk], [gtk+-3.0 >= 3.18.0])
PKG_CHECK_MODULES([vte], [vte-2.91 >= 0.28.0])

# Gettext
GETTEXT_PACKAGE="gtkterm"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GNU_GETTEXT_VERSION([0.19.7])
AM_GNU_GETTEXT([external])

# GLib Resource compiler
AC_SUBST([GLIB_COMPILE_RESOURCES], [`$PKG_CONFIG --variable glib_compile_resources gio-2.0`])

AC_ARG_WITH([lockdir],
            [AS_HELP_STRING([--with-lockdir],
                            [Use a different lockdir @<:@default=/var/lock/lockdev@:>@])],
            [],
            [with_lockdir="/var/lock/lockdev"])
AC_DEFINE_UNQUOTED(P_LOCK, "$with_lockdir", [Serial lockfile directory])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    po/Makefile.in])

AC_OUTPUT
