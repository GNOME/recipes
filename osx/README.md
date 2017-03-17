The materials in this folder can be used to build a dmg of Recipes on OS X.

The steps I've followed are:

* Use the [GTK-OSX](https://wiki.gnome.org/Projects/GTK+/OSX/Building) jhbuild setup to build GTK+
* Switch to the GNOME 3.24 moduleset and build a newer gtk+-3 and gnome-autoar
* Build recipes, configuring it with --disable-gspell and --disable-canberra (I couldn't get those to build)
* Install [gtk-mac-bundler](https://wiki.gnome.org/Projects/GTK+/OSX/Bundling)
* Run ```gtk-mac-bundler ./org.gnome.Recipes.bundle``` in a jhbuild shell. This should produce a Recipes.app folder
* Run ```./build-osx-installer.sh```. If all goes well, this produces a recipes.dmg image.

Possible complications:

* The template for the disk image might become too small again, as we are adding more recipes. It can be resized with the OS X disk utility.
