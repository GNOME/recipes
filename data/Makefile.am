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

mimedir = $(datadir)/mime/packages
dist_mime_DATA = org.gnome.Recipes-mime.xml

imagesdir = $(pkgdatadir)/images
soundsdir = $(pkgdatadir)/sounds

dist_images_DATA = \
	images/adelia.jpg		\
	images/adria.jpg		\
	images/afranke.jpg		\
	images/alex.png			\
	images/allan_day.jpg		\
	images/bagel.jpg		\
	images/Bastian.jpg		\
	images/chickencurry.jpg		\
	images/chickensoup.jpg		\
	images/ebassi-hackergotchi.png	\
	images/elvin.jpg		\
	images/erusan.gif		\
	images/fyksen.jpg		\
	images/geisha.jpg		\
	images/halfline.jpg		\
	images/mclasen.jpg		\
	images/mwleeds.jpg		\
	images/Sonic.jpg		\
	images/tigert.jpg		\
	images/borscht.jpg		\
	images/cake_aux_lardons.jpg	\
	images/cake_aux_lardons2.jpg	\
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
	images/ebassi-carbonara-cover.JPG	\
	images/ebassi-carbonara-01.JPG	\
	images/ebassi-carbonara-02.JPG	\
	images/ebassi-carbonara-03.JPG	\
	images/ebassi-carbonara-04.JPG	\
	images/ebassi-carbonara-05.JPG	\
	images/ebassi-carbonara-06.JPG	\
	images/feaneron.png		\
	images/gingered_carrot_soup.jpg	\
	images/grumbeerekiechle_1.JPG	\
	images/grumbeerekiechle_2.JPG	\
	images/grumbeerekiechle_3.JPG	\
	images/hazelnutmilk1.jpg	\
	images/hazelnutmilk2.jpg	\
	images/hazelnutmilk3.jpg	\
	images/hazelnutmilk4.jpg	\
	images/hazelnutmilk5.jpg	\
	images/hazelnutmilk6.jpg	\
	images/hazelnutmilk7.jpg	\
	images/hazelnutbutter1.jpg	\
	images/hazelnutbutter2.jpg	\
	images/hazelnutbutter3.jpg	\
	images/hazelnutbutter4.jpg	\
	images/kiwi-mandarin1.jpg	\
	images/kiwi-mandarin2.jpg	\
	images/kiwi-mandarin3.jpg	\
	images/kiwi-mandarin4.jpg	\
	images/kiwi-mandarin5.jpg	\
	images/kiwi-mandarin6.jpg	\
	images/kiwi-mandarin7.jpg	\
	images/mantovana.jpg		\
	images/non_knead_buns.png	\
	images/non_knead_buns1.jpg	\
	images/non_knead_buns2.jpg	\
	images/non_knead_buns3.jpg	\
	images/non_knead_buns4.jpg	\
	images/non_knead_buns5.jpg	\
	images/oats1.jpg		\
	images/oats2.jpg		\
	images/oats3.jpg		\
	images/pineapple_upside_down.jpg \
	images/pizzaburger.jpg		\
	images/rakost1.jpg		\
	images/rakost2.jpg		\
	images/rakost3.jpg		\
	images/rootspie1.jpg		\
	images/rootspie2.jpg		\
	images/rootspie3.jpg		\
	images/rootspie4.jpg		\
	images/rootspie5.jpg		\
	images/rootspie6.jpg		\
	images/rootspie7.jpg		\
	images/rootspie8.jpg		\
	images/rootspie9.jpg		\
	images/rootspie10.jpg		\
	images/rootspie11.jpg		\
	images/rootspie12.jpg		\
	images/rootspie13.jpg		\
	images/rosemary-potatos1.jpg	\
	images/rosemary-potatos2.jpg	\
	images/salmonrice1.jpg		\
	images/salmonrice2.jpg		\
	images/salmonrice3.jpg		\
	images/salmonrice4.jpg		\
	images/salmonrice5.jpg		\
	images/salmonrice6.jpg		\
	images/salmonrice7.jpg		\
	images/salmonrice8.jpg		\
	images/salmonrice9.jpg		\
	images/spanish_omelet1.jpg	\
	images/spanish_omelet2.jpg	\
	images/spanish_omelet3.jpg	\
	images/spanish_omelet4.jpg	\
	images/spanish_omelet5.jpg	\
	images/spanish_omelet6.jpg	\
	images/spanish_omelet7.jpg	\
	images/spanish_omelet8.jpg	\
	images/spanish_omelet9.jpg	\
	images/tofuscramble.jpg		\
	images/wholesale-crackers.jpg 	\
	images/pan_pizza1.png		\
	images/pan_pizza2.png		\
	images/pan_pizza3.png		\
	images/tagliatelle1.jpg		\
	images/tagliatelle2.jpg		\
	images/tagliatelle3.jpg		\
	images/tagliatelle4.jpg		\
	images/whitefish1.jpg		\
	images/whitefish2.jpg		\
	images/whitefish3.jpg		\
	images/whitefish4.jpg		\
	images/american.png		\
	images/chinese.png		\
	images/french.jpg		\
	images/indian.jpg		\
	images/italian.png		\
	images/mediterranean.png	\
	images/nordic.jpg		\
	images/turkish.png		\
	images/big-american.png		\
	images/big-chinese.png		\
	images/big-french.jpg		\
	images/big-indian.jpg		\
	images/big-italian.png		\
	images/big-mediterranean.png	\
	images/big-nordic.jpg		\
	images/big-turkish.png

dist_sounds_DATA = \
	sounds/complete.oga

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

install-data-hook: install-update-icon-cache install-update-mime-database
uninstall-hook: uninstall-update-icon-cache uninstall-update-mime-database

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

install-update-mime-database:
	$(AM_V_at)$(POST_INSTALL)
	if [ -f $(DESTDIR)$(datadir)/mime/packages/freedesktop.org.xml ] ; then \
		if which update-mime-database>/dev/null 2>&1; then \
			update-mime-database $(DESTDIR)$(datadir)/mime; \
		fi \
	fi

uninstall-update-mime-database:
	$(AM_V_at)$(POST_UNINSTALL)
	if [ -f $(DESTDIR)$(datadir)/mime/packages/freedesktop.org.xml ] ; then \
		if which update-mime-database>/dev/null 2>&1; then \
			update-mime-database $(DESTDIR)$(datadir)/mime; \
		fi \
	fi

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
	$(AM_V_GEN) mkdir -p appdata; msgfmt --xml -d $(top_srcdir)/po --template $< -o $@

EXTRA_DIST += $(appdata_in_files) appdata/org.gnome.Recipes.appdata.xml

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = org.gnome.Recipes-search-provider.ini

check: check-appstream

check-appstream: appdata/org.gnome.Recipes.appdata.xml
	$(APPSTREAM_UTIL) validate-relax $<
