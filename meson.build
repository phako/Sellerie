project(
    'Sellerie',
    'c',
    version: '1.0.0',
    default_options : ['c_std=gnu11'],
    meson_version : '>= 0.45.0'
)

i18n = import('i18n')
gnome = import('gnome')

cc = meson.get_compiler('c')

have_serial_h = cc.has_header('linux/serial.h')

prefix = get_option('prefix')

add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')
add_global_arguments('-DG_LOG_USE_STRUCTURED', language : 'c')
add_global_arguments(['-Wno-unused', '-Wno-unused-parameter', '-Wno-missing-field-initializers'], language : 'c')

gtk_deps = dependency('gtk+-3.0', version : '>= 3.0.0')
vte_deps = dependency('vte-2.91', version : '>= 0.28.0')
udev_deps = dependency('gudev-1.0', version: '>= 230', required: false)

conf = configuration_data()
conf.set('VERSION', '"@0@"'.format(meson.project_version()))
conf.set('GETTEXT_PACKAGE', '"sellerie"')
conf.set('PACKAGE', '"sellerie"')
conf.set('LOCALEDIR', '"@0@/@1@"'.format(prefix, get_option('localedir')))
conf.set('P_LOCK', '"@0@"'.format(get_option('lockdir')))

if have_serial_h
  conf.set('HAVE_LINUX_SERIAL_H', '1')
endif

if udev_deps.found()
  conf.set('HAVE_GUDEV', '1')
endif

configure_file(output : 'config.h', configuration : conf)
config = declare_dependency(include_directories : include_directories('.'))
install_man('sellerie.1')

subdir('data')
subdir('src')
subdir('po')
