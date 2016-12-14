-include $(top_srcdir)/git.mk

EXTRA_DIST =

desktopdir = $(datadir)/applications

desktop_in_files = org.gnome.Recipes.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	msgfmt --desktop -d $(top_srcdir)/po --template $< -o $@

EXTRA_DIST += $(desktop_in_files)

ingredientsdir = $(pkgdatadir)/ingredients
imagesdir = $(pkgdatadir)/images

ingredients_DATA = \
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

images_DATA = \
	images/alex.png			\
	images/bagel.jpg		\
	images/Bastian.jpg		\
	images/burger-fries.jpg		\
	images/coffee.jpg		\
	images/elvin.jpg		\
	images/food.jpg			\
	images/halfline.jpg		\
	images/mclasen.jpg		\
	images/meatballs.jpg		\
	images/pesto.jpg		\
	images/pizza.jpg		\
	images/plain-bagel.jpg		\
	images/salad.jpg		\
	images/spring-rolls.jpg		\
	images/tofuscramble.jpg		\
	images/wholesale-crackers.jpg

pkgdata_DATA = \
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

dist_appsicon16_DATA = icons/16x16/org.gnome.Recipes.png icons/16x16/org.gnome.Recipes-symbolic.symbolic.png
dist_appsicon24_DATA = icons/24x24/org.gnome.Recipes.png icons/24x24/org.gnome.Recipes-symbolic.symbolic.png
dist_appsicon32_DATA = icons/32x32/org.gnome.Recipes.png icons/32x32/org.gnome.Recipes-symbolic.symbolic.png
dist_appsicon48_DATA = icons/48x48/org.gnome.Recipes.png icons/48x48/org.gnome.Recipes-symbolic.symbolic.png
dist_appsicon512_DATA = icons/512x512/org.gnome.Recipes.png icons/512x512/org.gnome.Recipes-symbolic.symbolic.png
dist_appsiconsymbolic_DATA = icons/symbolic/org.gnome.Recipes-symbolic.svg

update_icon_cache = gtk-update-icon-cache --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache
	touch libcheese_la_vala.stamp

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Recipes.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

GITIGNOREFILES = $(service_DATA)

appdatadir = $(datadir)/appdata

appdata_in_files = appdata/org.gnome.Recipes.appdata.xml.in

appdata_DATA = $(appdata_in_files:.appdata.xml.in=.appdata.xml)

%.appdata.xml: %.appdata.xml.in
	msgfmt --xml -d $(top_srcdir)/po --template $< -o $@

EXTRA_DIST += $(appdata_in_files)
