# GtkTerm Makefile.am

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src po
man_MANS = gtkterm.1
EXTRA_DIST = TODO $(man_MANS)
CLEANFILES = $(desktop_DATA)
dist_noinst_DATA = org.jensge.GtkTerm.desktop.in

desktopdir = $(datadir)/applications
desktop_DATA = org.jensge.GtkTerm.desktop

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

icondir = $(datadir)/icons/hicolor/scalable/apps
icon_DATA = data/org.jensge.GtkTerm.svg

-include $(top_srcdir)/git.mk

GITIGNOREFILES = \
	m4/* \
	**/resource.c \
	**/*~ \
	aclocal.m4 \
	**/*.sw* \
	build-aux \
	config.h.in

@GENERATE_CHANGELOG_RULES@
dist-hook: dist-ChangeLog
