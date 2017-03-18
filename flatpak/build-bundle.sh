#! /bin/sh

BUILD_DIR=build
JSON=org.gnome.Recipes.json
REPO=repo

echo "Removing build dir..."
rm -rf $BUILD_DIR

echo "Making repository..."
ostree init --mode=archive-z2 --repo=$REPO

echo "Building with flatpak-builder..."
flatpak-builder --repo=$REPO $BUILD_DIR $JSON

echo "Creating bundle file..."
flatpak build-bundle $REPO org.gnome.Recipes.x86_64.flatpak org.gnome.Recipes master

