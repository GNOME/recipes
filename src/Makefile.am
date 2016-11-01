bin_PROGRAMS = recipes

recipes_CFLAGS = $(RECIPES_CFLAGS)
recipes_LDADD = $(RECIPES_LIBS)
recipes_SOURCES = \
	main.c 			\
	gr-app.h 		\
	gr-app.c 		\
	gr-window.h 		\
	gr-window.c 		\
	gr-recipes-page.h	\
	gr-recipes-page.c	\
	gs-hiding-box.h 	\
	gs-hiding-box.c 	\
	gr-recipe.c 		\
	gr-recipe.h 		\
	gr-author.c 		\
	gr-author.h 		\
	gr-recipe-store.c 	\
	gr-recipe-store.h	\
	gr-recipe-tile.c 	\
	gr-recipe-tile.h 	\
	gr-category-tile.c 	\
	gr-category-tile.h 	\
	gr-chef-tile.c 		\
	gr-chef-tile.h 		\
	gr-details-page.h	\
	gr-details-page.c	\
	gr-edit-page.h		\
	gr-edit-page.c		\
	gr-utils.h		\
	gr-utils.c		\
	gr-preferences.h	\
	gr-preferences.c	\
	gr-list-page.h		\
	gr-list-page.c		\
	gr-search-page.h	\
	gr-search-page.c	\
	gr-cuisines-page.h	\
	gr-cuisines-page.c	\
	gr-cuisine-tile.h	\
	gr-cuisine-tile.c	\
	gr-big-cuisine-tile.h	\
	gr-big-cuisine-tile.c	\
	gr-cuisine-page.h	\
	gr-cuisine-page.c	\
	gr-cuisine.h		\
	gr-cuisine.c		\
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
