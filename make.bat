@echo off
rescan
cycles 180000
REM PATH=d:\editor\bin;%PATH%

rem bcc -S -3 -f287 -w -w-sig -w-inl -h -ff -B -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc
rem bcc -B -c -3 -f287 -ff -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc
rem bcc -S -B -f287 -ff -3  -k- -w -w-sig -w-inl -P -d -a -Z -O -Oemvlpbig -mc -I../include -L../lib main.cc
rem bcc -S -3 -ff -k- -w -w-sig -w-inl -P -d -a -O -mc -I../include -L../lib main.cc
rem del e.exe
rem main.exe e.exe

REM GOTO BUILD_LARGE
GOTO BUILD_COMPACT

:BUILD_COMPACT
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -mc -I..\include vga.cc
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -mc -I..\include mario.cc
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -mc -I..\include kbhit.cc
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -mc -I..\include main.cc
d:\editor\bin\tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 main.asm,main.obj
d:\editor\bin\tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 mario.asm,mario.obj
d:\editor\bin\tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 kbhit.asm,kbhit.obj
d:\editor\bin\tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 vga.asm,vga.obj
d:\editor\bin\tasm /D__COMPACT__ /D__CDECL__ /r /ml /m9 cpu.asm,cpu.obj
d:\editor\bin\tlink /3 /m /c ..\lib\c0c.obj main.obj mario.obj vga.obj cpu.obj kbhit.obj,e.exe,,..\lib\cc.lib ..\lib\mathc.lib ..\lib\fp87.lib
GOTO BUILD_COMPLETE

:BUILD_LARGE
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -ml -I..\include vga.cc
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -ml -I..\include mario.cc
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -ml -I..\include kbhit.cc
d:\editor\bin\bcc -k- -S -P -ff -Ff -x- -RT- -w -w-sig -w-inl -d -a -O2 -Os -OW -f287 -4 -ml -I..\include main.cc
d:\editor\bin\tasm /D__LARGE__ /D__CDECL__ /r /ml /m9 main.asm,main.obj
d:\editor\bin\tasm /D__LARGE__ /D__CDECL__ /r /ml /m9 mario.asm,mario.obj
d:\editor\bin\tasm /D__LARGE__ /D__CDECL__ /r /ml /m9 kbhit.asm,kbhit.obj
d:\editor\bin\tasm /D__LARGE__ /D__CDECL__ /r /ml /m9 vga.asm,vga.obj
d:\editor\bin\tasm /D__LARGE__ /D__CDECL__ /r /ml /m9 cpu.asm,cpu.obj
d:\editor\bin\tlink /3 /m /c ..\lib\c0l.obj main.obj mario.obj vga.obj cpu.obj kbhit.obj,e.exe,,..\lib\cl.lib ..\lib\mathl.lib ..\lib\fp87.lib
GOTO BUILD_COMPLETE


:BUILD_COMPLETE

REM Compact model, i.e. 32-bit data pointers but 16-bit code pointers

REM c0c = normal startup module
REM c0fc = when -Fm or -Fs is used, stack is placed in DS so SS=DS

rem bcc -P -WX -ml -I..\inc45 -L..\lib\dpmi main.cc
REM rem bcc -S -f287 -ff -Ff -3 -w -w-sig -w-inl -P -d -a -O2 -Os -WX -ml -I..\inc45 main.cc
REM bcc -S -f287 -ff -Ff -3 -w -w-sig -w-inl -P -d -a -WX -ml -I..\inc45 main.cc
REM tasm /D__DPMI__ /D__LARGE__ /D__CDECL__ /r/ml/m9 main.asm,main.obj
REM tlink /Txe /3 /m /P=2 /yx ..\lib\dpmi\c0x.obj main.obj, e.exe,,..\lib\dpmi\dpmi16.lib ..\lib\dpmi\cwl.lib ..\lib\mathwl.lib
