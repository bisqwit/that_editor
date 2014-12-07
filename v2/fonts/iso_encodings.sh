pp="uniq | perl -pe 's/0x0*([0-9a-fA-F]+,)/0x\1/g'"

cd /usr/share/antiword/  || \
cd "$HOME"/src/charconv/unicode/ISO8859/

if [ ! -f 8859-1.TXT -a ! -f 8859-1.txt ]; then
	echo "Need a directory with 8859-1.TXT through 8859-16.TXT" >&2
	echo "Download these files from ISO8859/8859-*.TXT on ftp.unicode.org" >&2
	exit
fi

for s in `seq 1 16`;do \
	echo -ne "/* ISO-8859-$s */ $s => Array( "
	grep '^0x' 8859-$s.??? </dev/null \
	| perl -pe 's/(.*?)\t(.*?)\t.*/\1=>\2,/' \
	| tr -d '\012' \
	| sed 's/,$//'
	echo "),"
done | sort | sh -c "$pp"

