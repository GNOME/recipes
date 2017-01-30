bin_PROGRAMS = recipes

commitid := $(shell $(GIT) describe)
libgd_info = $(shell cat $(top_srcdir)/libgd.info)
libglnx_info = $(shell cat $(top_srcdir)/libglnx.info)

recipes_CFLAGS = \
	$(WARN_CFLAGS) 			\
	-Wno-sign-compare		\
	-O0 -ggdb			\
	$(RECIPES_CFLAGS) 		\
	-DPKGDATADIR=\"$(pkgdatadir)\" 	\
	-DLOCALEDIR=\"$(localedir)\" 	\
	-DCOMMIT_ID=\"$(commitid)\"	\
	-DLIBGD_INFO=\"$(libgd_info)\"	\
	-DLIBGLNX_INFO=\"$(libglnx_info)\"	\
	-DMICRO_VERSION=$(MICRO_VERSION) \
	-I "$(top_srcdir)/libgd"	\
	-I "$(top_srcdir)/libglnx"	\
	-I "$(top_srcdir)/src"		\
	-I "$(top_builddir)/src"

recipes_LDADD = $(RECIPES_LIBS)			\
		-lm				\
		$(top_builddir)/libgd/libgd.la

if USE_AUTOAR
recipes_LDADD += $(top_builddir)/libglnx.la
endif

recipes_SOURCES = \
	main.c 			\
	gr-app.h 		\
	gr-app.c 		\
	gr-category-tile.h 	\
	gr-category-tile.c 	\
	gr-chef.h		\
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
	gr-diet.h		\
	gr-diet.c		\
	gr-diet-row.h		\
	gr-diet-row.c		\
	gr-edit-page.h		\
	gr-edit-page.c		\
	gr-images.h		\
	gr-images.c		\
	gr-image-viewer.h	\
	gr-image-viewer.c	\
	gr-image-page.h		\
	gr-image-page.c		\
	gr-ingredient.h		\
	gr-ingredient.c		\
	gr-ingredient-row.h	\
	gr-ingredient-row.c	\
	gr-ingredients-list.h	\
	gr-ingredients-list.c	\
	gr-list-page.h		\
	gr-list-page.c		\
	gr-meal.h		\
	gr-meal.c		\
	gr-meal-row.h		\
	gr-meal-row.c		\
	gr-number.h		\
	gr-number.c		\
	gr-chef-dialog.h	\
	gr-chef-dialog.c	\
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
	gr-recipe-small-tile.h 	\
	gr-recipe-small-tile.c 	\
	gr-recipe-tile.h 	\
	gr-recipe-tile.c 	\
	gr-recipes-page.h	\
	gr-recipes-page.c	\
	gr-search-page.h	\
	gr-search-page.c	\
	gr-season.h		\
	gr-season.c		\
	gr-shopping-list-printer.h	\
	gr-shopping-list-printer.c	\
	gr-shopping-page.h	\
	gr-shopping-page.c	\
	gr-timer.h		\
	gr-timer.c		\
	gr-timer-widget.h	\
	gr-timer-widget.c	\
	gr-toggle-button.h	\
	gr-toggle-button.c	\
	gr-unit.h		\
	gr-unit.c		\
	gr-utils.h		\
	gr-utils.c		\
	gr-window.h 		\
	gr-window.c 		\
	resources-ui.c		\
	resources-images.c	\
	types.h			\
	types.c			\
	gr-shell-search-provider.h \
	gr-shell-search-provider.c \
	gr-shell-search-provider-dbus.h \
	gr-shell-search-provider-dbus.c

BUILT_SOURCES = \
	resources-ui.c		\
	resources-images.c	\
	ingredients.c		\
	no-ingredients.c	\
	segments.c		\
	types.h			\
	types.c			\
	gr-shell-search-provider-dbus.h \
	gr-shell-search-provider-dbus.c

DISTCLEANFILES = \
	ingredients.c		\
	no-ingredients.c	\
	segments.c

MAINTAINERCLEANFILES = \
	resources-ui.c		\
	resources-images.c	\
	types.h			\
	types.c			\
	gr-shell-search-provider-dbus.h \
	gr-shell-search-provider-dbus.c

segments.c: $(top_srcdir)/data/segments.list
	$(AM_V_GEN) $(SED) -e 's/^\(.*\)$$/	N_("\1"),/' $^ > $@

ingredients.c: $(top_srcdir)/data/ingredients.list
	$(AM_V_GEN) $(SED) -e 's/^\(.*\)$$/	N_("\1"),/' $^ > $@

no-ingredients.c: $(top_srcdir)/data/ingredients.list
	$(AM_V_GEN) $(SED) -e 's/^\(.*\)$$/	N_("no \1"),/' $^ > $@

resource_ui_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/recipes-ui.gresource.xml)

resources-ui.c: recipes-ui.gresource.xml $(resource_ui_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/recipes-ui.gresource.xml \
                --target=$@ --sourcedir=$(srcdir) --generate-source

resource_image_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/recipes-images.gresource.xml)

resources-images.c: recipes-images.gresource.xml $(resource_image_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/recipes-images.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

types.h: types.h.template gr-diet.h
	$(AM_V_GEN) ($(GLIB_MKENUMS) --template $^ ) > xgen-eh && \
	cp xgen-eh $@ && \
	rm -f xgen-eh

types.c: types.c.template gr-diet.h
	$(AM_V_GEN) ($(GLIB_MKENUMS) --template $^ ) > xgen-ec && \
	cp xgen-ec $@ && \
	rm -f xgen-ec

gr-shell-search-provider-dbus.h gr-shell-search-provider-dbus.c: Makefile.am $(srcdir)/shell-search-provider-dbus-interfaces.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome. \
		--c-namespace Gr \
		--generate-c-code gr-shell-search-provider-dbus \
		$(srcdir)/shell-search-provider-dbus-interfaces.xml

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = org.gnome.Recipes-search-provider.ini

EXTRA_DIST = \
	shell-search-provider-dbus-interfaces.xml 	\
	recipes-ui.gresource.xml 			\
	recipes-images.gresource.xml 			\
	$(resource_ui_files)				\
	$(resource_image_files)				\
	types.h.template				\
	types.c.template

-include $(top_srcdir)/git.mk
