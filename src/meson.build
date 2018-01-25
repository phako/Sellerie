sources = [
    'term_config.h',
    'term_config.c',
    'fichier.c',
    'gtkterm.c',
    'fichier.h',
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
    'main-window.h',
    'main-window.c',
    resources
]

all_deps = [gtk_deps, vte_deps, udev_deps, config]
sellerie = executable('sellerie', sources,
                      link_args : '-rdynamic',
                      install : true,
                      dependencies : all_deps)
