t="cp_encodings.tmp"

n=0
trap="rm -f '$t'" ; trap "$trap" 0


pp="uniq | perl -pe 's/0x0*([0-9a-fA-F]+,)/0x\1/g'"

fifo=0
rm -f "$t"
if mkfifo "$t"; then
	sort < "$t" | sh -c "$pp" &
	trap="$trap;kill -9 $!" ; trap "$trap" 0
	fifo=1
fi

for d in /usr/share/antiword/  "$HOME"/src/charconv/unicode/VENDORS/{*,*/*}/; do

for p in "$d"{CP,cp}*.{txt,TXT}; do

if [ -f "$p" ]; then

	s="$(echo "$p"|sed 's@.*/@@'|tr -dc 0-9.|tr . ' '|sed 's/^0*//')"
	
	echo -ne "/* CP$s */ $s => Array( "
	grep '^0x' "$p" \
	| perl -pe 's/(.*?)\t(.*?)\t.*/\1=>\2,/' \
	| tr -d '\012' \
	| sed 's/,$//'
	echo "),"
	
	n=$((n+1))
fi
done

done > "$t"

if [ $fifo = 0 ]; then
	sort < "$t" | sh -c "$pp"
fi

if [ $n = 0 ]; then
	echo "Need a directory with CP437.TXT etc." >&2
	echo "Download these files from VENDORS/*/CP*.TXT on ftp.unicode.org" >&2
fi
