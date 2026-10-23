#!/bin/bash
# install/mkpkg.sh - the SHSUCD package for the GeosOne Dual Installer
# (https://github.com/geosone/gdi): SHSUCDX, CDAUDIO, SHSUCDHD, MKCDM.
#
#   install/mkpkg.sh      -> build/pkg, build/shsucd.zip, build/SHSUCD.IMG
#
# Environment:
#   WATCOM   Open Watcom 2.0 (default /opt/watcom): MKCDM.EXE
#   LINK     an OMF linker for SHSUCDHD.EXE (default jwlink)
#   GDI      INSTALL.EXE of the GeosOne Dual Installer
#            (default ../gdi/build/INSTALL.EXE)
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
export WATCOM=${WATCOM:-/opt/watcom}
export PATH=$WATCOM/binl:$PATH INCLUDE=$WATCOM/h
LINK=${LINK:-jwlink}
GDI=${GDI:-$ROOT/../gdi/build/INSTALL.EXE}
B=$ROOT/build
PKG=$B/pkg
crlf() { sed 's/\r*$/\r/' "$1" > "$2"; }

rm -rf "$PKG" "$B/obj" && mkdir -p "$PKG" "$B/obj"
cd "$ROOT"
nasm -O9 -w-all -o "$PKG/SHSUCDX.COM" shsucdx.nsm
nasm -f bin -o "$PKG/CDAUDIO.COM" cdaudio.asm
nasm -O9 -w-all -fobj -o "$B/obj/shsucdhd.obj" shsucdhd.nsm
"$LINK" option quiet format dos name "$PKG/SHSUCDHD.EXE" file "$B/obj/shsucdhd.obj"
(cd "$B/obj" && wcl -q -bt=dos -ms -fe="$PKG/MKCDM.EXE" "$ROOT/mkcdm.c")
cp install/INSTALL.INI "$PKG/"
sed -i 's/\r*$/\r/' "$PKG/INSTALL.INI"
crlf install/README.TXT "$PKG/README.TXT"
crlf readme.txt "$PKG/SHSUCD.TXT"
crlf shsucdx.txt "$PKG/SHSUCDX.TXT"
crlf LICENSE.txt "$PKG/SHSUCD.LIC"
cp "$GDI" "$PKG/INSTALL.EXE"

cd "$PKG"
rm -f ../shsucd.zip && zip -q -9 ../shsucd.zip *
rm -f ../SHSUCD.IMG && mformat -C -f 1440 -v SHSUCD -i ../SHSUCD.IMG :: && mcopy -i ../SHSUCD.IMG * ::
du -sh "$PKG" ../shsucd.zip
