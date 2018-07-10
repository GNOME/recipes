custom_target('recipes.db.h',
              input: ['recipes.db'],
              output: ['recipes.db.h'],
              command: [recipe_extract, '@INPUT@'],
              capture: true,
              build_by_default: true)

custom_target('chefs.db.h',
              input: ['chefs.db'],
              output: ['chefs.db.h'],
              command: [recipe_extract, '--chefs', '@INPUT@'],
              capture: true,
              build_by_default: true)

install_data(   'images/american.png',
                'images/big-american.png',
                'images/big-chinese.png',
                'images/big-french.jpg',
                'images/big-indian.jpg',
                'images/big-italian.png',
                'images/big-mediterranean.png',
                'images/big-nordic.jpg',
                'images/big-turkish.png',
                'images/chinese.png',
                'images/french.jpg',
                'images/indian.jpg',
                'images/italian.png',
                'images/mediterranean.png',
                'images/nordic.jpg',
                'images/turkish.png',
                install_dir: join_paths([pkgdatadir,'images']))

install_data('sounds/complete.oga',
             install_dir: join_paths([pkgdatadir,'sounds']))

install_data('chefs.db',
             'picks.db',
             'recipes.db',
             install_dir: pkgdatadir)

install_data('org.gnome.Recipes-mime.xml',
             install_dir: join_paths([datadir, 'mime/packages']))

search_provider_conf = configuration_data()

search_provider_conf.set('appid', application_id)
search_provider_conf.set('profile', profile)

configure_file(
  configuration: search_provider_conf,
  input: files('org.gnome.Recipes-search-provider.ini.in'),
  install_dir: join_paths(datadir, 'gnome-shell', 'search-providers'),
  output: '@0@.search-provider.ini'.format(application_id)
)

msgfmt = find_program('msgfmt')

desktop_conf = configuration_data()

desktop_conf.set('icon', application_id)

custom_target('desktop-file',
              output: '@@@.desktop'.format(application_id),
              input: files('org.gnome.Recipes.desktop.in'),
              input: configure_file(
                input: files('org.gnome.Recipes.desktop.in.in'),
                output: 'org.gnome.Recipes.desktop.in',
                configuration: desktop_conf
              ),
              install: true,
              install_dir: join_paths([datadir,'applications']),
              command: [msgfmt, '--desktop', '-d', '../po', '--template', '@INPUT@', '-o', '@OUTPUT@'])


appdata_conf = configuration_data()

appdata_conf.set('appid', application_id)

custom_target('appdata',
              output: '@@@.appdata.xml'.format(application_id),
              input: configure_file(
                input: files(join_paths('appdata', 'org.gnome.Recipes.appdata.xml.in.in')),
                output: 'org.gnome.Recipes.appdata.xml.in',
                configuration: appdata_conf
              ),
              install: true,
              install_dir: join_paths([datadir,'metainfo']),
              command: [msgfmt, '--xml', '-d', '../po', '--template', '@INPUT@', '-o', '@OUTPUT@'])

service_conf = configuration_data()

service_conf.set('appid', application_id)
service_conf.set('bindir', join_paths(prefix, bindir))

configure_file(input: 'org.gnome.Recipes.service.in',
               output: '@@@.service'.format(application_id),
               configuration: service_conf,
               install_dir: join_paths([datadir,'dbus-1/services']))

gnome.compile_schemas(build_by_default: true)

install_data('org.gnome.Recipes.gschema.xml',
             install_dir: schemadir)

foreach icon_size: ['16x16', '24x24', '32x32', '48x48', '512x512']
  icondir = join_paths('icons', icon_size)
  icondest_dir = join_paths('icons', 'hicolor', icon_size, 'app')

  install_data(
    join_paths(icondir, 'org.gnome.Recipes.png'),
    install_dir: join_paths(datadir, icon_destdir),
    rename: '@0@.png'.format(application_id)
  )

  install_data(
    join_paths(icondir, 'org.gnome.Recipes-symbolic.symbolic.png'),
    install_dir: join_paths(datadir, icon_destdir),
    rename: '@0@-symbolic.symbolic.png'.format(application_id)
  )
endforeach

icondir = join_paths('icons', 'symbolic')
icondest_dir = join_paths('icons', 'hicolor', 'symbolic', 'apps')

install_data(
  join_paths(icondir, 'org.gnome.Recipes-symbolic.svg'),
  install_dir: join_paths(datadir, icon_destdir),
  rename: '@0@-symbolic.svg'.format(application_id)
)
