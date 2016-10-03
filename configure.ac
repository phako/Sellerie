# GtkTerm autoconf script

AC_INIT([GtkTerm], [1.0], [https://github.com/phako/gtkterm])
AM_INIT_AUTOMAKE([foreign -Wall -Wno-portability])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])

GETTEXT_PACKAGE="gtkterm"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
IT_PROG_INTLTOOL([0.40.0])
AM_GLIB_GNU_GETTEXT

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

PKG_CHECK_MODULES([gtk], [gtk+-3.0 >= 3.0.0])
PKG_CHECK_MODULES([vte], [vte-2.91 >= 0.28.0])

# Linux is required for arbitrary baud rates
AC_CHECK_HEADERS_ONCE([linux/serial.h])
AC_SUBST([GLIB_COMPILE_RESOURCES], [`$PKG_CONFIG --variable glib_compile_resources gio-2.0`])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    po/Makefile.in])

AC_OUTPUT
