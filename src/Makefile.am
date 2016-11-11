bin_PROGRAMS = recipes

recipes_CFLAGS = $(RECIPES_CFLAGS) -DPKG_DATA_DIR=\"$(pkgdatadir)\" -O0 -ggdb
recipes_LDADD = $(RECIPES_LIBS) -lm
recipes_SOURCES = \
	main.c 			\
	gr-app.h 		\
	gr-app.c 		\
	gr-big-cuisine-tile.h	\
	gr-big-cuisine-tile.c	\
	gr-category.h		\
	gr-category.c		\
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
	gr-edit-page.h		\
	gr-edit-page.c		\
	gs-hiding-box.h 	\
	gs-hiding-box.c 	\
	gr-images.h		\
	gr-images.c		\
	gr-ingredient.h		\
	gr-ingredient.c		\
	gr-ingredient-search-tile.h	\
	gr-ingredient-search-tile.c	\
	gr-ingredient-tile.h	\
	gr-ingredient-tile.c	\
	gr-ingredients-list.h	\
	gr-ingredients-list.c	\
	gr-ingredients-search-page.h	\
	gr-ingredients-search-page.c	\
	gr-ingredients-page.h	\
	gr-ingredients-page.c	\
	gr-list-page.h		\
	gr-list-page.c		\
	gr-preferences.h	\
	gr-preferences.c	\
	gr-recipe.h 		\
	gr-recipe.c 		\
	gr-recipe-store.h 	\
	gr-recipe-store.c 	\
	gr-recipe-tile.h 	\
	gr-recipe-tile.c 	\
	gr-recipes-page.h	\
	gr-recipes-page.c	\
	gr-search-page.h	\
	gr-search-page.c	\
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
