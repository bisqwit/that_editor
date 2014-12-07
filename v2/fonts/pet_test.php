<?php

$data = file_get_contents('characters.901225-01.bin');

for($m=0; $m<0x40; $m+=8)
{
  for($y=0; $y<8; ++$y)
  {
    printf("%02X  ", $m);
    for($n=$m; $n< $m+8; ++$n)
    {
      $tgt_num = 0x40 + $n;
      $src_pos = 0x40*8 + $n*8;

      $ch = ord($data[$src_pos+$y]);
      for($x=0; $x<8; ++$x)
      {
        $p = $ch & (1 << (7-$x));
        print $p ? '#' : '.';
      }
      print '  ';
    }
    print "\n";
  }
  print "\n";
}