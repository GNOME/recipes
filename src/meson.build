src = []
src_incs = []
enums = []

# These generated files are #included
list_to_c = find_program('list_to_c.py')
foreach f : ['ingredients']
  ofile = f + '.inc'
  ifile = files('../data/' + f + '.list')
  src_incs += [custom_target(f,
                             output : ofile,
                             input : ifile,
                             command : [list_to_c, '@INPUT@', '@OUTPUT@', ''])]
endforeach

src += src_incs

ofile = 'no-ingredients.inc'
ifile = files('../data/ingredients.list')
src += [custom_target('no-ingredients',
                      output : ofile,
                      input : ifile,
                      command : [list_to_c, '@INPUT@', '@OUTPUT@', 'no '])]

# Resource compilation
resources = gnome.compile_resources('resources',
                                    'recipes.gresource.xml',
                                    c_name: '_recipes',
                                    source_dir: 'src')

enums += gnome.mkenums('types',
                       sources: [ 'gr-diet.h', 'gr-unit.h' ],
                       c_template: 'types.c.template',
                       h_template: 'types.h.template')

search_provider = gnome.gdbus_codegen('gr-shell-search-provider-dbus',
                                      'shell-search-provider-dbus-interfaces.xml',
                                      interface_prefix: 'org.gnome.',
                                      namespace: 'Gr')

libsrc = [
       'gr-number.c',
       'gr-unit.c',
       'gr-utils.c'
]

librecipes = static_library('recipes', libsrc,
                            install : false,
                            include_directories : top_inc,
                            dependencies: deps)

src += ['main.c',
       'gr-about-dialog.c',
       'gr-account.c',
       'gr-app.c',
       'gr-appdata.c',
       'gr-category-tile.c',
       'gr-chef.c',
       'gr-chef-dialog.c',
       'gr-chef-tile.c',
       'gr-convert-units.c',
       'gr-cooking-page.c',
       'gr-cooking-view.c',
       'gr-cuisine.c',
       'gr-cuisine-page.c',
       'gr-cuisine-tile.c',
       'gr-cuisines-page.c',
       'gr-cuisine-tile.c',
       'gr-cuisines-page.c',
       'gr-details-page.c',
       'gr-diet.c',
       'gr-diet-row.c',
       'gr-edit-page.c',
       'gr-gourmet-format.c',
       'gr-image-page.c',
       'gr-image.c',
       'gr-image-viewer.c',
       'gr-image-page.c',
       'gr-ingredient.c',
       'gr-ingredient-row.c',
       'gr-ingredients-list.c',
       'gr-ingredients-viewer.c',
       'gr-ingredients-viewer-row.c',
       'gr-list-page.c',
       'gr-logging.c',
       'gr-mail.c',
       'gr-meal.c',
       'gr-meal-row.c',
       'gr-query-editor.c',
       'gr-recipe.c',
       'gr-recipe-exporter.c',
       'gr-recipe-formatter.c',
       'gr-recipe-importer.c',
       'gr-recipe-printer.c',
       'gr-shopping-tile.c',
       'gr-recipe-store.c',
       'gr-recipe-tile.c',
       'gr-recipes-page.c',
       'gr-search-page.c',
       'gr-season.c',
       'gr-settings.c',
       'gr-shell-search-provider.c',
       'gr-shopping-list-exporter.c',
       'gr-shopping-list-formatter.c',
       'gr-shopping-list-printer.c',
       'gr-shopping-page.c',
       'gr-spice-row.c',
       'gr-time-widget.c',
       'gr-timer.c',
       'gr-timer-widget.c',
       'gr-window.c',
  enums,
  search_provider,
  resources]

executable('gnome-recipes',
           src,
           link_with: librecipes,
           install : true,
           include_directories : top_inc,
           dependencies: deps)
