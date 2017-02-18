project('gnome-recipes', 'c', version: '0.1.0',
        meson_version : '>=0.36.0')

i18n = import('i18n')

conf = configuration_data()

conf.set_quoted('G_LOG_DOMAIN', 'org.gnome.Recipes')
conf.set_quoted('PACKAGE_NAME', 'gnome-recipes')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', 'gnome-recipes')
conf.set_quoted('PKGDATADIR', join_paths([ get_option('prefix'),
                                           get_option('datadir'),
                                           'gnome-recipes' ]))
conf.set_quoted('LOCALEDIR', join_paths([ get_option('prefix'),
                                          get_option('datadir'),
                                          'locale' ]))

configure_file(output : 'config.h', configuration : conf)

# Needed to find config.h
top_inc = include_directories('.')

subdir('po')
libgd = subproject('libgd',
           default_options: [
                'with-tagged-entry=true',
                'static=true'
           ])
libgd_dep = libgd.get_variable('libgd_dep')

deps = [ dependency('gtk+-3.0'),
         dependency('gnome-autoar-0'),
         dependency('gspell-1'),
         dependency('libcanberra'),
         libgd_dep ]

subdir('src')
