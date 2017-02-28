#!/bin/sh

mkdir -p mnttmp
rm -f working.dmg
gunzip < template.dmg.gz > working.dmg
hdiutil attach working.dmg -noautoopen -quiet -mountpoint mnttmp
# NOTE: output of hdiutil changes every now and then.
# Verify that this is still working.
DEV=`hdiutil info|tail -1|awk '{print $1}'`
rm -rf mnttmp/Recipes.app
mv ./Recipes.app mnttmp
hdiutil detach ${DEV}
rm -rf mnttmp
rm -f recipes.dmg
hdiutil convert working.dmg -quiet -format UDZO -imagekey zlib-level=9 -o recipes.dmg
rm -f working.dmg
