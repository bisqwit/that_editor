@rem bcc -pr -S -3 -f287 -h -Ff -B -P -d -a -O -Oemvlpbig -Z -mc -I../include -L../lib main.cc >> tmptmp
bcc -pr -k- -w -winl- -P -d -a -O -3 -mc -I../include -L../lib main.cc > tmptmp
type tmptmp

