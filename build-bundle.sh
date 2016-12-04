#! /bin/sh

BUILD_DIR=build
JSON=org.gnome.Recipes.flatpak.json
REPO=repo

echo "Removing build dir..."
rm -rf $BUILD_DIR

echo "Building with flatpak-builder..."
flatpak-builder --repo=$REPO $BUILD_DIR $JSON

echo "Creating bundle file..."
flatpak build-bundle $REPO org.gnome.Recipes.x86_64.flatpak org.gnome.Recipes master

