-include $(top_srcdir)/git.mk

EXTRA_DIST =
CLEANFILES =

EXTRA_DIST += ingredients.list segments.list recipes.db.h chefs.db.h

recipes.db.h: recipes.db
	$(AM_V_GEN) $(top_builddir)/tools/recipe-extract $^ > $@

chefs.db.h: chefs.db
	$(AM_V_GEN) $(top_builddir)/tools/recipe-extract --chefs $^ > $@

desktopdir = $(datadir)/applications

desktop_in_files = org.gnome.Recipes.desktop.in

dist_desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) msgfmt --desktop -d $(top_srcdir)/po --template $< -o $@

EXTRA_DIST += $(desktop_in_files)

ingredientsdir = $(pkgdatadir)/ingredients
imagesdir = $(pkgdatadir)/images

dist_ingredients_DATA = \
	ingredients/Almond.png 		\
	ingredients/Amaretti.png	\
	ingredients/Anchovis.png	\
	ingredients/Apple.png		\
	ingredients/Apricot.png		\
	ingredients/Artichoke.png	\
	ingredients/Asparagus.png	\
	ingredients/Aubergine.png	\
	ingredients/Bacon.png 		\
	ingredients/Bagel.png		\
	ingredients/Banana.png		\
	ingredients/Basil.png		\
	ingredients/Biscotti.png 	\
	ingredients/Carrot.png

dist_images_DATA = \
	images/alex.png			\
	images/allan_day.jpg		\
	images/bagel.jpg		\
	images/Bastian.jpg		\
	images/burger-fries.jpg		\
	images/coffee.jpg		\
	images/elvin.jpg		\
	images/erusan.gif		\
	images/food.jpg			\
	images/fyksen.jpg		\
	images/halfline.jpg		\
	images/mclasen.jpg		\
	images/donauwellen0.jpg		\
	images/donauwellen1.jpg		\
	images/donauwellen2.jpg		\
	images/donauwellen3.jpg		\
	images/donauwellen4.jpg		\
	images/donauwellen5.jpg		\
	images/donauwellen6.jpg		\
	images/donauwellen7.jpg		\
	images/donauwellen8.jpg		\
	images/donauwellen_final.jpg	\
	images/meatballs.jpg		\
	images/pesto.jpg		\
	images/pineapple_upside_down.jpg \
	images/pizzaburger.jpg		\
	images/pizza.jpg		\
	images/plain-bagel.jpg		\
	images/salad.jpg		\
	images/spring-rolls.jpg		\
	images/tofuscramble.jpg		\
	images/wholesale-crackers.jpg 	\
	images/pan_pizza1.png		\
	images/pan_pizza2.png		\
	images/pan_pizza3.png

dist_pkgdata_DATA = \
	chefs.db	\
	picks.db	\
	recipes.db

iconthemedir = $(datadir)/icons/hicolor

appsicon16dir = $(iconthemedir)/16x16/apps
appsicon24dir = $(iconthemedir)/24x24/apps
appsicon32dir = $(iconthemedir)/32x32/apps
appsicon48dir = $(iconthemedir)/48x48/apps
appsicon512dir = $(iconthemedir)/512x512/apps
appsiconsymbolicdir = $(iconthemedir)/symbolic/apps

dist_appsicon16_DATA = \
	icons/16x16/org.gnome.Recipes.png 			\
	icons/16x16/org.gnome.Recipes-symbolic.symbolic.png

dist_appsicon24_DATA = \
	icons/24x24/org.gnome.Recipes.png 			\
	icons/24x24/org.gnome.Recipes-symbolic.symbolic.png

dist_appsicon32_DATA = \
	icons/32x32/org.gnome.Recipes.png 			\
	icons/32x32/org.gnome.Recipes-symbolic.symbolic.png

dist_appsicon48_DATA = \
	icons/48x48/org.gnome.Recipes.png 			\
	icons/48x48/org.gnome.Recipes-symbolic.symbolic.png

dist_appsicon512_DATA = \
	icons/512x512/org.gnome.Recipes.png 			\
	icons/512x512/org.gnome.Recipes-symbolic.symbolic.png

dist_appsiconsymbolic_DATA = \
	icons/symbolic/org.gnome.Recipes-symbolic.svg

update_icon_cache = gtk-update-icon-cache --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Recipes.service.in
dist_service_DATA = $(service_in_files:.service.in=.service)

$(dist_service_DATA): $(service_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

EXTRA_DIST += $(service_in_files)

CLEANFILES += $(dist_service_DATA)

GITIGNOREFILES = $(dist_service_DATA)

appdatadir = $(datadir)/appdata

appdata_in_files = appdata/org.gnome.Recipes.appdata.xml.in

dist_appdata_DATA = $(appdata_in_files:.appdata.xml.in=.appdata.xml)

%.appdata.xml: %.appdata.xml.in
	$(AM_V_GEN) mkdir appdata; msgfmt --xml -d $(top_srcdir)/po --template $< -o $@

EXTRA_DIST += $(appdata_in_files)
