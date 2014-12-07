all: 16x32.inc 8x19.inc 8x32.inc c64font.inc 6x9.inc

16x32.inc: 16x32.bdf bdf_to_font.php
	php bdf_to_font.php -f"$<" -o"$@" -nfont16x32

8x16.inc: vga8x16.bdf bdf_to_font.php
	php bdf_to_font.php -f"$<" -o"$@" -nfont8x16

8x19.inc: vga8x19.bdf bdf_to_font.php
	php bdf_to_font.php -f"$<" -o"$@" -nfont8x19

8x32.inc: 8x32.bdf bdf_to_font.php
	php bdf_to_font.php -f"$<" -o"$@" -nfont8x32

6x9.inc: 6x9.bdf bdf_to_font.php
	php bdf_to_font.php -f"$<" -o"$@" -nfont6x9

c64font.inc: characters.901225-01.bin pet_to_font.php
	php pet_to_font.php -f"$<" -o"$@" -nc64font


iso_encodings.php.inc: iso_encodings.sh
	./"$<" > "$@"

cp_encodings.php.inc: cp_encodings.sh
	./"$<" > "$@"
