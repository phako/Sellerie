# GtkTerm Makefile.am

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src po
man_MANS = gtkterm.1
EXTRA_DIST = TODO $(man_MANS)

-include $(top_srcdir)/git.mk

GITIGNOREFILES = \
	m4/* \
	**/resource.c \
	**/*~ \
	aclocal.m4 \
	**/*.sw* \
	build-aux \
	config.h.in
