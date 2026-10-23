# SHSUCD with digital CD audio (GeosOne)

The SHSUCD suite of Jason Hood ([adoxa/shsucd](https://github.com/adoxa/shsucd), see [readme.txt](readme.txt)) with changes. This is an altered version, not the original:

- **CD audio** (PCGamingTimeMachine): SHSUCDX 4.00 hands PLAY, STOP and RESUME to a digital player such as [VSBHDA](https://github.com/geosone/VSBHDA), which reads the audio sectors from the drive. Games then play their CD music on drives without an analog cable (SATA, USB, virtual). SHSUCDHD 4.00 plays the audio tracks of CUE/BIN images from XMS (`MKCDM` makes the track table).
- **GeosOne:** `SHSUCDX /A:ON|OFF|AUTO` and `CDAUDIO.COM` to show or switch the mode at run time; a setup package.

| Mode | PLAY goes to |
|---|---|
| `/A:ON` | VSBHDA (digital) |
| `/A:OFF` | the drive (analog), as the original SHSUCDX |
| `/A:AUTO` | VSBHDA while it is loaded, else the drive |

## Setup

Download the ZIP (or the 1.44 MB disk image) of the [latest release](https://github.com/geosone/shsucd/releases), unpack it and run `INSTALL.EXE` (the [GeosOne DOS Installer](https://github.com/geosone/gdi)). It copies SHSUCDX, CDAUDIO, SHSUCDHD and MKCDM to `C:\OPENASPI` and puts SHSUCDX in the place of `MSCDEX` (MS-DOS), `NWCDEX` (DR-DOS) or another SHSUCDX in `AUTOEXEC.BAT`: the line keeps its CD-ROM drivers (`/D:`) and drive letters (SHSUCDX ignores the MSCDEX options `/E /K /S /M`), with `/A:AUTO` added. `INSTALL /U` in `C:\OPENASPI\SHSUCD` puts MSCDEX or NWCDEX back.

[AHCIASPI](https://github.com/geosone/ahciaspi), [EUSBASPI](https://github.com/geosone/eusbaspi) and [OpenASPI](https://github.com/geosone/OpenAspi) bring SHSUCDX as well; all of them share `C:\OPENASPI`.

## Building

Linux: NASM, Open Watcom 2.0 (`MKCDM.EXE`), an OMF linker (JWlink, `LINK=path`, for `SHSUCDHD.EXE`), mtools and the GeosOne DOS Installer 1.14 (`../gdi/build/INSTALL.EXE`, or `GDI=path`):

    install/mkpkg.sh      # build/pkg, build/shsucd.zip, build/SHSUCD.IMG
    test/install.sh       # QEMU: MS-DOS 7.1 with MSCDEX -> setup -> SHSUCDX -> INSTALL /U

`test/install.sh` needs a MS-DOS 7.1 boot floppy with XMGR.SYS, UDVD2.SYS and MSCDEX.EXE (`BOOTIMG`). The original DOS makefile (`makefile`, Borland tools) is unchanged.

## License

[LICENSE.txt](LICENSE.txt) (zlib style). `INSTALL.EXE` in the package: GNU GPL 3.
