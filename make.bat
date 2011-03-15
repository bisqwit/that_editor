@echo off

rem bcc -S -3 -f287 -w -w-sig -w-inl -h -ff -B -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc

rem bcc -B -c -3 -f287 -ff -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc
bcc -B -f287 -ff -3  -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc
rem bcc -B -3 -ff -k- -w -w-sig -w-inl -P -d -a -O -mc -I../include -L../lib main.cc
rem tlink main.obj ..\lib\c0l.obj, e.exe,,..\lib\cl.lib ..\lib\mathl.lib ..\lib\fp87.lib
del e.exe
ren main.exe e.exe
