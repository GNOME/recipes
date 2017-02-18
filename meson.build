project('gnome-recipes', 'c', version: '0.1.0',
        meson_version : '>=0.36.0')

i18n = import('i18n')

conf = configuration_data()

# Needed to find config.h
top_inc = include_directories('.')

subdir('po')
libgd = subproject('libgd',
           default_options: [
                'with-tagged-entry=true',
                'static=true'
           ])
libgd_dep = libgd.get_variable('libgd_dep')

# Extract LIBGD_INFO
libgd_path = join_paths([meson.source_root(), 'subprojects', 'libgd'])
git_run = run_command('git', ['-C', libgd_path, 'log', '-1', '--pretty=%h'])
if git_run.returncode() == 0
  conf.set_quoted('LIBGD_INFO', git_run.stdout().strip())
else
  message('Failed to extract git commit hash for libgd:\n' + git_run.stderr())
  conf.set_quoted('LIBGD_INFO', 'unknown')
endif

deps = [ dependency('gtk+-3.0'),
         dependency('gnome-autoar-0'),
         dependency('gspell-1'),
         dependency('libcanberra'),
         libgd_dep ]

pkgdatadir = join_paths([ get_option('prefix'),
                          get_option('datadir'),
                          'gnome-recipes' ])
conf.set_quoted('G_LOG_DOMAIN', 'org.gnome.Recipes')
conf.set_quoted('PACKAGE_NAME', 'gnome-recipes')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', 'gnome-recipes')
conf.set_quoted('PKGDATADIR', pkgdatadir)
conf.set_quoted('LOCALEDIR', join_paths([ get_option('prefix'),
                                          get_option('datadir'),
                                          'locale' ]))
# Used while generating cuisine.css
conf.set('pkgdatadir', pkgdatadir)

configure_file(output : 'config.h', configuration : conf)

subdir('src')
