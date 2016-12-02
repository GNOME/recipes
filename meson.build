project('recipes', 'c', version: '0.1.0')

conf = configuration_data()

conf.set('G_LOG_DOMAIN', '"org.gnome.Recipes"')
conf.set('PACKAGE_NAME', '"recipes"')
conf.set('PACKAGE_VERSION', '"' + meson.project_version() + '"')
conf.set('GETTEXT_PACKAGE', '"recipes"')
conf.set('PKGDATADIR', '"' + get_option('prefix') + '/' + get_option('datadir') + '/recipes"')
conf.set('LOCALEDIR', '"' + get_option('prefix') + '/' + get_option('datadir') + '/locale"')

configure_file(output : 'config.h', configuration : conf)

libgd_tagged_entry = true
libgd_static = true
subdir('libgd')

deps = [ dependency('gtk+-3.0'),
         libgd_dep ]

gnome = import('gnome')

resources = gnome.compile_resources('resources',
                                    'src/recipes.gresource.xml',
                                    c_name: '_recipes',
                                    source_dir: 'src')

enums = gnome.mkenums('types',
                      sources: 'src/gr-recipe.h',
                      c_template: 'types.c.template',
                      h_template: 'types.h.template')


src =  ['src/main.c',
	'src/gr-app.h',
	'src/gr-app.c',
	'src/gr-big-cuisine-tile.h',
	'src/gr-big-cuisine-tile.c',
	'src/gr-category-tile.h',
	'src/gr-category-tile.c',
	'src/gr-chef.h',
	'src/gr-chef.c',
	'src/gr-chef-tile.h',
	'src/gr-chef-tile.c',
	'src/gr-cuisine.h',
	'src/gr-cuisine.c',
	'src/gr-cuisine-page.h',
	'src/gr-cuisine-page.c',
	'src/gr-cuisine-tile.h',
	'src/gr-cuisine-tile.c',
	'src/gr-cuisines-page.h',
	'src/gr-cuisines-page.c',
	'src/gr-details-page.h',
	'src/gr-details-page.c',
	'src/gr-diet-row.h',
	'src/gr-diet-row.c',
	'src/gr-edit-page.h',
	'src/gr-edit-page.c',
	'src/gr-images.h',
	'src/gr-images.c',
	'src/gr-ingredient.h',
	'src/gr-ingredient.c',
	'src/gr-ingredient-row.h',
	'src/gr-ingredient-row.c',
	'src/gr-ingredients-list.h',
	'src/gr-ingredients-list.c',
	'src/gr-ingredients-page.h',
	'src/gr-ingredients-page.c',
	'src/gr-list-page.h',
	'src/gr-list-page.c',
	'src/gr-meal.h',
	'src/gr-meal.c',
	'src/gr-meal-row.h',
	'src/gr-meal-row.c',
	'src/gr-preferences.h',
	'src/gr-preferences.c',
	'src/gr-query-editor.h',
	'src/gr-query-editor.c',
	'src/gr-recipe.h',
	'src/gr-recipe.c',
	'src/gr-recipe-printer.h',
	'src/gr-recipe-printer.c',
	'src/gr-recipe-store.h',
	'src/gr-recipe-store.c',
	'src/gr-recipe-tile.h',
	'src/gr-recipe-tile.c',
	'src/gr-recipes-page.h',
	'src/gr-recipes-page.c',
	'src/gr-search-page.h',
	'src/gr-search-page.c',
	'src/gr-season.h',
	'src/gr-season.c',
	'src/gr-timer.h',
	'src/gr-timer.c',
	'src/gr-timer-widget.h',
	'src/gr-timer-widget.c',
	'src/gr-toggle-button.h',
	'src/gr-toggle-button.c',
	'src/gr-utils.h',
	'src/gr-utils.c',
	'src/gr-window.h',
	'src/gr-window.c',
        enums,
        resources]

executable('recipe',
           src,
           dependencies: deps)
