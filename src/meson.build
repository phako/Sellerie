enum_headers = files('serial-port.h', 'term_config.h')
enums = gnome.mkenums_simple ('sellerie-enums', sources : enum_headers)
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
    'main-window.h',
    'main-window.c',
    'infobar.h',
    'infobar.c',
    'serial-view.h',
    'serial-view.c',
    resources,
    enum_headers,
    enums
]

all_deps = [gtk_deps, vte_deps, udev_deps, config]
sellerie = executable('sellerie', sources,
                      export_dynamic : true,
                      install : true,
                      dependencies : all_deps)
