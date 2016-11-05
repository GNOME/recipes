
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
