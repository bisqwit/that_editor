all: 16x32.inc 8x19.inc 8x32.inc c64font.inc

16x32.inc: 16x32.php 16x32.bdf
	php "$<" > "$@"

8x19.inc: 8x19.php vga8x19.bdf
	php "$<" > "$@"

8x32.inc: 8x32.php 8x32.bdf
	php "$<" > "$@"

c64font.inc: petscii.php characters.901225-01.bin
	php "$<" > "$@"
