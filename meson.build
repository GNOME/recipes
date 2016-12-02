project('recipes', 'c', version: '0.1.0',
        meson_version : '>=0.36.0')

i18n = import('i18n')

conf = configuration_data()

conf.set_quoted('G_LOG_DOMAIN', 'org.gnome.Recipes')
conf.set_quoted('PACKAGE_NAME', 'recipes')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', 'recipes')
conf.set_quoted('PKGDATADIR', join_paths([ get_option('prefix'),
                                           get_option('datadir'),
                                           'recipes' ]))
conf.set_quoted('LOCALEDIR', join_paths([ get_option('prefix'),
                                          get_option('datadir'),
                                          'locale' ]))

configure_file(output : 'config.h', configuration : conf)

# Needed to find config.h
top_inc = include_directories('.')

libgd_tagged_entry = true
libgd_static = true
subdir('po')
subdir('libgd')

deps = [ dependency('gtk+-3.0'),
         dependency('gnome-autoar-0'),
         libgd_dep ]

subdir('src')
