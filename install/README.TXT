SHSUCD - CD-ROM redirector with digital CD audio for VSBHDA
===========================================================

SHSUCDX and SHSUCDHD 4.00: the SHSUCD suite 3-7 of Jason Hood
(https://github.com/adoxa/shsucd) with changes - an altered version, not
the original:

  - CD audio support by PCGamingTimeMachine: SHSUCDX hands PLAY, STOP and
    RESUME to a digital player (VSBHDA), which reads the audio sectors from
    the drive; SHSUCDHD plays audio tracks of CD images from XMS
  - GeosOne: /A:ON, /A:OFF, /A:AUTO and CDAUDIO.COM; this setup

What Setup did
--------------
The programs are in the installation directory (C:\OPENASPI). In
AUTOEXEC.BAT, the line that started MSCDEX, NWCDEX or another SHSUCDX
starts this SHSUCDX now, with its /D: drivers and drive letters (SHSUCDX
ignores the MSCDEX options /E /K /S /M); option 2 added /A:AUTO. Without
such a line, add one yourself, e.g.:

  C:\OPENASPI\SHSUCDX.COM /D:MSCD001 /A:AUTO

(MSCD001: the name the CD-ROM driver in CONFIG.SYS got with /D:).

CD audio
--------
Many games play their music from audio tracks of the CD. When the drive has
no analog cable to the sound card (SATA, USB, virtual drives), VSBHDA can
play them digitally: start VSBHDA with /CDA1 (https://github.com/geosone/VSBHDA).

  SHSUCDX /A:ON     PLAY goes to VSBHDA (digital)
  SHSUCDX /A:OFF    the drive plays (analog), as the original SHSUCDX
  SHSUCDX /A:AUTO   digital while VSBHDA is loaded, else the drive
  CDAUDIO [ON | OFF | AUTO]   shows or switches the mode at run time

CD images
---------
  SHSUCDHD /F:GAME.ISO          an ISO image as a CD-ROM drive (SHSU-CDH)
  MKCDM GAME.CUE GAME.CDM       a CUE/BIN image with audio tracks ...
  SHSUCDHD /F:GAME.CDM          ... is loaded into XMS, with CD audio
  SHSUCDX /D:SHSU-CDH /A:AUTO   gives it a drive letter

SHSUCDX.TXT is the manual of SHSUCDX, SHSUCD.TXT the readme of the suite;
this file stays as CDSETUP.TXT. INSTALL /U in C:\OPENASPI\SHSUCD puts
MSCDEX or NWCDEX back into AUTOEXEC.BAT.

License
-------
SHSUCD: zlib-style license of Jason Hood (SHSUCD.LIC). INSTALL.EXE
(GeosOne Dual Installer): GNU General Public License 3.
