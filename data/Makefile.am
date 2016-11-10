
-include $(top_srcdir)/git.mk

desktopdir = $(datadir)/applications

desktop_in_files = org.gnome.Recipes.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	msgfmt --desktop -d $(top_srcdir)/po --template $< -o $@

ingredientsdir = $(pkgdatadir)/ingredients

ingredients_DATA = \
	Almond.png 	\
	Amaretti.png	\
	Anchovis.png	\
	Apple.png	\
	Apricot.png	\
	Artichoke.png	\
	Asparagus.png	\
	Aubergine.png	\
	Bacon.png 	\
	Bagel.png	\
	Banana.png	\
	Basil.png	\
	Biscotti.png 	\
	Carrot.png

pkgdata_DATA = \
	picks.db

iconthemedir = $(datadir)/icons/hicolor

appsicon16dir = $(iconthemedir)/16x16/apps
appsicon24dir = $(iconthemedir)/24x24/apps
appsicon32dir = $(iconthemedir)/32x32/apps
appsicon48dir = $(iconthemedir)/48x48/apps
appsicon512dir = $(iconthemedir)/512x512/apps
appsiconsymbolicdir = $(iconthemedir)/symbolic/apps

dist_appsicon16_DATA = icons/16x16/org.gnome.Recipes.png
dist_appsicon24_DATA = icons/24x24/org.gnome.Recipes.png
dist_appsicon32_DATA = icons/32x32/org.gnome.Recipes.png
dist_appsicon48_DATA = icons/48x48/org.gnome.Recipes.png
dist_appsicon512_DATA = icons/512x512/org.gnome.Recipes.png
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
service_DATA = org.gnome.Recipes.service
