#!/bin/sh
test -n "$srcdir" || srcdir=$1
test -n "$srcdir" || srcdir=.

cd $srcdir

VERSION=$(git describe --abbrev=0)
NAME="nautilus-$VERSION"

echo "Updating submodules…"
git submodule update --init

echo "Creating git tree archive…"
git archive --prefix="${NAME}/" --format=tar HEAD > nautilus.tar

cd libgd

git archive --prefix="${NAME}/libgd/" --format=tar HEAD > libgd.tar

cd ..

rm -f "${NAME}.tar"

tar -Af "${NAME}.tar" nautilus.tar
tar -Af "${NAME}.tar" libgd/libgd.tar

rm -f nautilus.tar
rm -f libgd/libgd.tar

echo "Compressing archive…"
xz -f "${NAME}.tar"
