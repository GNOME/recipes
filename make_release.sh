#!/bin/sh
test -n "$srcdir" || srcdir=$1
test -n "$srcdir" || srcdir=.

cd $srcdir

VERSION=$(git describe --abbrev=0)
NAME="gnome-recipes-$VERSION"

echo "Updating submodules…"
git submodule update --init

echo "Creating git tree archive…"
git archive --prefix="${NAME}/" --format=tar HEAD > main.tar

cd subprojects/libgd

git archive --prefix="${NAME}/subprojects/libgd/" --format=tar HEAD > libgd.tar

cd ../..

rm -f "${NAME}.tar"

tar -Af "${NAME}.tar" main.tar
tar -Af "${NAME}.tar" subprojects/libgd/libgd.tar

rm -f main.tar
rm -f subprojects/libgd/libgd.tar

echo "Compressing archive…"
xz -f "${NAME}.tar"
