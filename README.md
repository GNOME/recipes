[![pipeline status](https://gitlab.gnome.org/GNOME/recipes/badges/master/pipeline.svg)](https://gitlab.gnome.org/GNOME/recipes/commits/master)

GNOME Recipes
=============

<p align="center">
  <img src="https://gitlab.gnome.org/GNOME/recipes/raw/master/data/icons/512x512/org.gnome.Recipes.png" alt="Recipes icon"/>
</p>

This app is about cooking and recipes.

The main objects of interest are

- Recipes
- Ingredients
- Cuisines
- Chefs

The design can be found here: https://wiki.gnome.org/Design/Apps/Recipes

Other information can be found here: https://wiki.gnome.org/Apps/Recipes

Building
--------

Dependencies (at least): meson gtk+-3 gnome-autoar-0 gspell-1 libcanberra itstool

On Fedora
```
sudo dnf install meson itstool gtk3-devel gnome-autoar-devel gnome-online-accounts-devel gspell-devel libcanberra-devel libsoup-devel
```

After the 1.0 release, Recipes has switched to exclusively use meson as build system.

To build Recipes from git, use the following steps: (note that the ninja tools is
called ninja-build on Fedora)

```
git clone --recursive https://gitlab.gnome.org/GNOME/recipes.git
cd recipes
rm -rf build
meson --prefix=<your prefix> build
ninja -C build
ninja -C build install
```

Note that the install step is optional. Recipes works just fine uninstalled, like this:

```
GSETTINGS_SCHEMA_DIR=build/data ./build/src/gnome-recipes
```

jhbuild also knows how to build recipes.

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
brew install meson
```
- MacOS Sierra and Gettext
On Sierra (at the least) gettext formula is keg only, in order to let the configuration script
to find msgfmt you will need to link the gettext package into the system. If you think this might
break other builds I would recommend to unlink (``brew unlink gettext``) after the compilation process.
```
brew link --force gettext
```

- Install GTK+ and other dependencies:
```
brew install gtk+3
brew install adwaita-icon-theme
brew install gspell
```
- Clone from git:
```
git clone --recursive https://gitlab.gnome.org/GNOME/recipes.git
```
- Build from git as usual, disabling some problematic dependencies:
```
cd recipes
meson -Dautoar=no -Dcanberra=no build
ninja -C build
```
To build a released version of recipes on OS X, just

```
brew install recipes
```

jhbuild has also been used successfully to build recipes on OS X.

Testing
-------

If you don't feel like building from source yourself, you can use Flatpak, like this:

```
flatpak install --from https://gitlab.gnome.org/GNOME/recipes/raw/master/flatpak/gnome-recipes.flatpakref
```

If you are lucky, just clicking this [link](https://gitlab.gnome.org/GNOME/recipes/raw/master/flatpak/gnome-recipes.flatpakref) will do the right thing.

After installing the Flatpak, the applications will show up in the GNOME shell overview, but you can also launch it from the commandline:

```
flatpak run org.gnome.Recipes
```

There is also a [recipes.dmg](https://download.gnome.org/binaries/mac/recipes/) installer for OS X.
