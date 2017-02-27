GNOME Recipes
=============

<p align="center">
  <img src="https://github.com/matthiasclasen/gr/blob/master/data/icons/512x512/org.gnome.Recipes.png?raw=true" alt="Recipes icon"/>
</p>

This app is about cooking and recipes.

The main objects of interest are

- recipes
- ingredients
- cuisines
- chefs

The design can be found here: https://wiki.gnome.org/Design/Apps/Recipes

Building
--------

Dependencies (at least): autoconf-archive gobject-introspection json-glib-1.0 gnome-autoar-0 gspell-1 libcanberra libappstream-glib

On debian stretch/sid:
```
apt install autoconf-archive gobject-introspection libjson-glib-dev libgnome-autoar-0-dev libgspell-1-dev libcanberra-dev libappstream-glib-dev
```

To build GNOME Recipes from git, use the following steps:

```
git clone --recursive git://git.gnome.org/recipes
cd recipes
./autogen.sh --prefix=<your preferred location>
make
make install
```
I list `make install` as the last step here, but Recipes works fine uninstalled as well.

Testing
-------

If you don't feel like building from source yourself, you can use Flatpak, like this:

```
flatpak install --from https://matthiasclasen.github.io/recipes-releases/gnome-recipes.flatpakref
```

After installing the Flatpak, the applications will show up in the GNOME shell overview, but you can also launch it from the commandline:

```
flatpak run org.gnome.Recipes
```

Other platforms
---------------

GNOME recipes has been successfully built on OS X. The following steps should get you there:

- Install [Homebrew](http://brew.sh/)
- Install autotools:
```
brew install autoconf
brew install automake
brew install libtool
brew install gettext
brew install pkg-config
```
- Install GTK+ and other dependencies:
```
brew install gtk+3
brew install adwaita-icon-theme
brew install gspell
```
- Clone from git:
```
git clone git://git.gnome.org/recipes
```
- Build from git as usual, disabling some problematic dependencies:
```
cd recipes
./autogen --disable-autoar --disable-canberra
make
```
To build a released version of recipes on OS X, just

```
brew install recipes
```
