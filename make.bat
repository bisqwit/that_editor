@echo off

rem bcc -S -3 -f287 -w -w-sig -w-inl -h -Ff -B -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc


rem bcc -B -3 -f287 -Ff -k- -w -w-sig -w-inl -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc

bcc -B -Ff -k- -w -w-sig -w-inl -P -d -a -O -3 -mc -I../include -L../lib main.cc

rem tlink main.obj ..\lib\c0l.obj, main.exe,,..\lib\cl.lib ..\lib\mathl.lib ..\lib\fp87.lib
