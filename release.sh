#!/bin/bash
set -e
git pull
VERSION=$(cat src/main.cpp|grep "const char .version"| awk -v FS="(\")" '{print $2}')
LASTTAG=$(git describe --tags --abbrev=0)
git log $LASTTAG..HEAD --no-decorate --pretty=format:"- %s" --abbrev-commit > changes.md
vim changes.md
read -p "Release $VERSION ? " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  rm changes.md
  exit 1
fi
cp .pio/build/nodemcu-32s/firmware.bin .pio/build/nodemcu-32s/firmware-$VERSION.bin
gh release create $VERSION .pio/build/nodemcu-32s/firmware-$VERSION.bin -F changes.md -t $VERSION
rm changes.md

