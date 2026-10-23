#!/bin/bash
# Test the setup package (install/mkpkg.sh -> build/pkg) in QEMU: DOS 7.1
# from a floppy (BOOTIMG, with XMGR.SYS, UDVD2.SYS and MSCDEX.EXE), the package on a
# hard disk C:, an ISO image as IDE CD-ROM:
#   boot 1: MSCDEX gives the CD a letter; INSTALL /S from C:\PKG to
#           C:\OPENASPI puts SHSUCDX in its place in A:\AUTOEXEC.BAT
#   boot 2: SHSUCDX (with /A:AUTO) gives the CD the letter, CDAUDIO shows
#           the mode
#   boot 3: INSTALL /U: MSCDEX is back
#
#   test/install.sh
#
# Output (build/test-install/): serial1..3.log, screen1..3.png
set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/test-install
BOOTIMG=${BOOTIMG:-$HOME/Iso/Dos/dos71de/Dos7.1_System_deutsch.IMG}
[ -f "$ROOT/build/pkg/INSTALL.EXE" ] || { echo "no build/pkg (install/mkpkg.sh)"; exit 1; }
rm -rf "$OUT"; mkdir -p "$OUT/iso"
crlf() { tr '|' '\n' | sed 's/$/\r/'; }

cp "$BOOTIMG" "$OUT/a.img"; chmod u+w "$OUT/a.img"
printf 'org 100h\nmov dx,501h\nmov al,0\nout dx,al\nint 20h\n' > "$OUT/qexit.asm"
nasm -f bin -o "$OUT/QEXIT.COM" "$OUT/qexit.asm" || exit 1
echo 'DEVICE=A:\XMGR.SYS|DOS=HIGH|DEVICE=A:\UDVD2.SYS /D:MSCD001|FILES=30|LASTDRIVE=Z' | crlf > "$OUT/CONFIG.SYS"
# setup adds its lines before the last line: A:\RUN.BAT does the steps
echo '@ECHO OFF|LH A:\MSCDEX.EXE /D:MSCD001 /L:R /M:10|A:\RUN.BAT' | crlf > "$OUT/AUTOEXEC.BAT"
echo '@ECHO OFF|ECHO --- R:> AUX|DIR R:\ > AUX|ECHO --- setup > AUX|C:\PKG\INSTALL.EXE /S:A:\ANS.INI > AUX|ECHO --- AUTOEXEC.BAT > AUX|TYPE A:\AUTOEXEC.BAT > AUX|A:\QEXIT' | crlf > "$OUT/RUN1.BAT"
echo '@ECHO OFF|ECHO --- R:> AUX|DIR R:\ > AUX|ECHO --- CDAUDIO > AUX|C:\OPENASPI\CDAUDIO > AUX|A:\QEXIT' | crlf > "$OUT/RUN2.BAT"
echo '@ECHO OFF|ECHO --- INSTALL /U > AUX|C:\OPENASPI\SHSUCD\INSTALL.EXE /U > AUX|ECHO --- AUTOEXEC.BAT > AUX|TYPE A:\AUTOEXEC.BAT > AUX|A:\QEXIT' | crlf > "$OUT/RUN3.BAT"
echo '[Answers]|Dir=C:\OPENASPI' | crlf > "$OUT/ANS.INI"
cp "$OUT/RUN1.BAT" "$OUT/RUN.BAT"
mcopy -o -i "$OUT/a.img" "$OUT/CONFIG.SYS" "$OUT/AUTOEXEC.BAT" "$OUT/RUN.BAT" "$OUT/ANS.INI" "$OUT/QEXIT.COM" :: || exit 1

# C: 32 MB FAT16 with the package
truncate -s 32M "$OUT/c.img"
echo '63,,6,*' | sfdisk -q "$OUT/c.img" >/dev/null || exit 1
C="$OUT/c.img@@32256"
mformat -i "$C" -T 65473 -h 16 -s 63 -H 63 :: || exit 1
mmd -i "$C" ::/PKG && mcopy -i "$C" "$ROOT"/build/pkg/* ::/PKG/ || exit 1

printf 'CD test file\r\n' > "$OUT/iso/CDTEST.TXT"
xorriso -as mkisofs -quiet -V SHSUTEST -o "$OUT/cd.iso" "$OUT/iso" 2>/dev/null || exit 1

boot() {   # n [keys after 10 s]
    {
        if [ -n "${2:-}" ]; then sleep 10; for k in $2; do echo "sendkey $k"; sleep 2; done; fi
        sleep 25; echo "screendump $OUT/screen$1.ppm"; sleep 1; echo quit
    } | qemu-system-i386 -machine pc ${ACCEL:--accel kvm} -m 16 -display none -vga std -monitor stdio \
        -drive if=floppy,format=raw,file="$OUT/a.img" -drive if=ide,index=0,format=raw,file="$OUT/c.img" \
        -drive if=ide,index=2,media=cdrom,file="$OUT/cd.iso" -boot a -nic none \
        -serial file:"$OUT/serial$1.log" -device isa-debug-exit,iobase=0x501,iosize=0x02 > "$OUT/monitor$1.log" 2>&1
}
boot 1
mcopy -o -i "$OUT/a.img" "$OUT/RUN2.BAT" ::/RUN.BAT
boot 2
mcopy -o -i "$OUT/a.img" "$OUT/RUN3.BAT" ::/RUN.BAT
boot 3 "r ret ret"
for f in "$OUT"/*.ppm; do [ -f "$f" ] && magick "$f" "${f%.ppm}.png" && rm -f "$f"; done
for n in 1 2 3; do tr -d '\r' < "$OUT/serial$n.log"; done

python3 - "$OUT" <<'PYEOF'
import sys
o = sys.argv[1]
log = [open(f"{o}/serial{n}.log", "rb").read().decode("latin-1").replace("\r", "") for n in (1, 2, 3)]
def autoexec(t):
    return [l.strip() for l in t.split("--- AUTOEXEC.BAT")[-1].split("\n") if l.strip()]
inst = autoexec(log[0])
assert "C:\\OPENASPI\\SHSUCDX.COM /D:MSCD001 /L:R /M:10 /A:AUTO" in inst, inst
assert "CDTEST" in log[0].split("--- setup")[0], "MSCDEX: no R:"
assert "CDTEST" in log[1].split("--- CDAUDIO")[0], "SHSUCDX: no R:"
assert "AUTO" in log[1].split("--- CDAUDIO")[1].upper(), "CDAUDIO"
assert "LH A:\\MSCDEX.EXE /D:MSCD001 /L:R /M:10" in autoexec(log[2]), autoexec(log[2])
print("SHSUCD setup test passed")
PYEOF
