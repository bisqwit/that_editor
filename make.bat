@echo off

rem bcc -S -3 -f287 -w -w-sig -w-inl -h -ff -B -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc
rem bcc -B -c -3 -f287 -ff -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc
rem bcc -S -B -f287 -ff -3  -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc > tmptmp
rem bcc -B -3 -ff -k- -w -w-sig -w-inl -P -d -a -O -mc -I../include -L../lib main.cc
rem del e.exe
rem main.exe e.exe

bcc -S -f287 -ff -Ff -3 -w -w-sig -w-inl -P -d -a -O2 -Os -mc -I../include main.cc
tasm /D__COMPACT__ /D__CDECL__ /r/ml/m9 main.asm,main.obj
tlink ..\lib\c0c.obj main.obj, e.exe,,..\lib\cc.lib ..\lib\mathc.lib ..\lib\fp87.lib
