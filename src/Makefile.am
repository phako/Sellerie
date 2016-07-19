# GtkTerm src/Makefile.am

AM_CFLAGS = $(gtk_CFLAGS) $(vte_CFLAGS)

bin_PROGRAMS = gtkterm

nodist_gtkterm_SOURCES = \
	resource.c

resource_depends = \
	$(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies \
		--sourcedir=$(top_srcdir)/data $(top_srcdir)/data//org.jensge.GtkTerm.gresource.xml)

resource.c: $(top_srcdir)/data/org.jensge.GtkTerm.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(top_srcdir)/data --generate $<

gtkterm_SOURCES = \
    term_config.c \
    fichier.c \
    gtkterm.c \
    serie.c \
    config.h \
    fichier.h \
    serie.h \
    widgets.c \
    widgets.h \
    cmdline.c \
    cmdline.h \
    parsecfg.c \
    parsecfg.h \
    buffer.c \
    buffer.h \
    macros.c \
    macros.h \
    i18n.c \
    i18n.h \
    auto_config.h \
    logging.c \
    logging.h 

gtkterm_LDADD = $(gtk_LIBS) $(vte_LIBS)

CLEANFILES = *~


AM_CPPFLAGS = -DLOCALEDIR=\""$(localedir)"\"
