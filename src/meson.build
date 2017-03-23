ffiles = gnome.compile_resources('resource',
                                   '../data/org.jensge.GtkTerm.gresource.xml',
                                   source_dir: '../data')
sources = [
    'term_config.h',
    'term_config.c',
    'fichier.c',
    'gtkterm.c',
    'fichier.h',
    'widgets.c',
    'widgets.h',
    'cmdline.c',
    'cmdline.h',
    'buffer.c',
    'buffer.h',
    'macros.c',
    'macros.h',
    'i18n.c',
    'i18n.h',
    'logging.c',
    'logging.h',
    'parsecfg.c',
    'parsecfg.h',
    'serial-port.c',
    'serial-port.h',
    ffiles
]

all_deps = [gtk_deps, vte_deps, udev_deps, config]
gtkterm = executable('gtkterm', sources,
                     install : true,
                     dependencies : all_deps)
