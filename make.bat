@echo off
cycles 80000
REM PATH=d:\editor\bin;%PATH%

rem bcc -S -3 -f287 -w -w-sig -w-inl -h -ff -B -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc
rem bcc -B -c -3 -f287 -ff -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc
rem bcc -S -B -f287 -ff -3  -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc > tmptmp
rem bcc -S -3 -ff -k- -w -w-sig -w-inl -P -d -a -O -mc -I../include -L../lib main.cc
rem del e.exe
rem main.exe e.exe

bcc -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -mc -I..\include main.cc
tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 main.asm,main.obj
tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 cpu.asm,cpu.obj
tlink /3 /m /P=65536 /yx+ ..\lib\c0c.obj main.obj cpu.obj, e.exe,,..\lib\cc.lib ..\lib\mathc.lib ..\lib\fp87.lib

REM c0c = normal startup module
REM c0fc = when -Fm or -Fs is used, stack is placed in DS so SS=DS

rem bcc -P -WX -ml -I..\inc45 -L..\lib\dpmi main.cc
REM rem bcc -S -f287 -ff -Ff -3 -w -w-sig -w-inl -P -d -a -O2 -Os -WX -ml -I..\inc45 main.cc
REM bcc -S -f287 -ff -Ff -3 -w -w-sig -w-inl -P -d -a -WX -ml -I..\inc45 main.cc
REM tasm /D__DPMI__ /D__LARGE__ /D__CDECL__ /r/ml/m9 main.asm,main.obj
REM tlink /Txe /3 /m /P=2 /yx ..\lib\dpmi\c0x.obj main.obj, e.exe,,..\lib\dpmi\dpmi16.lib ..\lib\dpmi\cwl.lib ..\lib\mathwl.lib
