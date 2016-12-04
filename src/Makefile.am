bin_PROGRAMS = recipes

recipes_CFLAGS = \
	$(WARN_CFLAGS) 			\
	-Wno-sign-compare		\
	$(RECIPES_CFLAGS) 		\
	-DPKGDATADIR=\"$(pkgdatadir)\" 	\
	-DLOCALEDIR=\"$(localedir)\" 	\
	-I "$(top_srcdir)/libgd"	\
	-I "$(top_srcdir)/libglnx"

recipes_LDADD = $(RECIPES_LIBS)			\
		-lm				\
		$(top_builddir)/libgd/libgd.la	\
		$(top_builddir)/libglnx.la

recipes_SOURCES = \
	main.c 			\
	gr-app.h 		\
	gr-app.c 		\
	gr-category-tile.h 	\
	gr-category-tile.c 	\
	gr-chef.h 		\
	gr-chef.c 		\
	gr-chef-tile.h 		\
	gr-chef-tile.c 		\
	gr-cuisine.h		\
	gr-cuisine.c		\
	gr-cuisine-page.h	\
	gr-cuisine-page.c	\
	gr-cuisine-tile.h	\
	gr-cuisine-tile.c	\
	gr-cuisines-page.h	\
	gr-cuisines-page.c	\
	gr-details-page.h	\
	gr-details-page.c	\
	gr-diet-row.h		\
	gr-diet-row.c		\
	gr-edit-page.h		\
	gr-edit-page.c		\
	gr-images.h		\
	gr-images.c		\
	gr-ingredient.h		\
	gr-ingredient.c		\
	gr-ingredient-row.h	\
	gr-ingredient-row.c	\
	gr-ingredient-tile.h	\
	gr-ingredient-tile.c	\
	gr-ingredients-list.h	\
	gr-ingredients-list.c	\
	gr-ingredients-page.h	\
	gr-ingredients-page.c	\
	gr-list-page.h		\
	gr-list-page.c		\
	gr-meal.h		\
	gr-meal.c		\
	gr-meal-row.h		\
	gr-meal-row.c		\
	gr-preferences.h	\
	gr-preferences.c	\
	gr-query-editor.h	\
	gr-query-editor.c	\
	gr-recipe.h 		\
	gr-recipe.c 		\
	gr-recipe-exporter.h 	\
	gr-recipe-exporter.c	\
	gr-recipe-importer.h 	\
	gr-recipe-importer.c	\
	gr-recipe-printer.h	\
	gr-recipe-printer.c	\
	gr-recipe-store.h 	\
	gr-recipe-store.c 	\
	gr-recipe-tile.h 	\
	gr-recipe-tile.c 	\
	gr-recipes-page.h	\
	gr-recipes-page.c	\
	gr-search-page.h	\
	gr-search-page.c	\
	gr-season.h		\
	gr-season.c		\
	gr-timer.h		\
	gr-timer.c		\
	gr-timer-widget.h	\
	gr-timer-widget.c	\
	gr-toggle-button.h	\
	gr-toggle-button.c	\
	gr-utils.h		\
	gr-utils.c		\
	gr-window.h 		\
	gr-window.c 		\
	resources.c		\
	types.h			\
	types.c

BUILT_SOURCES = \
	resources.c		\
	types.h			\
	types.c

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/recipes.gresource.xml)

resources.c: recipes.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/recipes.gresource.xml \
                --target=$@ --sourcedir=$(srcdir) --generate-source

types.h: types.h.template gr-recipe.h
	$(AM_V_GEN) ($(GLIB_MKENUMS) --template types.h.template gr-recipe.h ) > xgen-eh && \
	cp xgen-eh $@ && \
	rm -f xgen-eh

types.c: types.c.template gr-recipe.h
	$(AM_V_GEN) ($(GLIB_MKENUMS) --template types.c.template gr-recipe.h ) > xgen-ec && \
	cp xgen-ec $@ && \
	rm -f xgen-ec

EXTRA_DIST = \
	$(resource_files)	\
	types.h.template	\
	types.c.template

-include $(top_srcdir)/git.mk
