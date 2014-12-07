for n in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    for m in `seq 18 37`; do
	rm test.o syntax.o
	CPPFLAGS="-DNUM_DISTINCT=$n -DMAX_DISTINCT=$m" make -j6 test &>/dev/null
	mv test "./test-$n-$m"
	(
	  use=`valgrind ./test-$n-$m 2>&1|grep 'heap usage'|sed 's/.*frees, *//;s/ .*//;s/,//'`
	  printf "%07d NUM=%d MAX=%d\n" $use $n $m
	)&
    done
done
