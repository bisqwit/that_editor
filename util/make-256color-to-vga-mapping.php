<?php

$im = ImageCreate(1,1);

function Alloc($r,$g,$b)
{
  global $im;
  #print "$r $g $b\t\t";
  $r = pow($r / 31, 1.6)*255;
  $g = pow($g / 31, 1.6)*255;
  $b = pow($b / 31, 1.6)*255;
  #print "$r $g $b\n";
  ImageColorAllocate($im, $r,$g,$b);
}

$tab = Array();
for($n=0; $n<16; ++$n)
{
  $r = ($n&8) ? (5 + ($n&1)/1 * 26) : (0 + ($n&1)/1 * 21);
  $g = ($n&8) ? (5 + ($n&2)/2 * 26) : (0 + ($n&2)/2 * 21);
  $b = ($n&8) ? (5 + ($n&4)/4 * 26) : (0 + ($n&4)/4 * 21);
  if($n==3) { $g = 5; }
  if($n==8) { $r=$g=$b = 7; }
  $tab[] = Array($b,$g,$r);
  Alloc($b,$g,$r);
}

$gramp = Array(1,2,3,5,6,7,8,9,11,12,13,14,16,17,18,19,20,22,23,24,25,27,28,29);
$cramp = Array(0,12,16,21,26,31);
for($r=0; $r<6; ++$r) for($g=0; $g<6; ++$g) for($b=0; $b<6; ++$b)
  $tab[] = Array($cramp[$r],$cramp[$g],$cramp[$b]);
for($n=0; $n<24; ++$n) $tab[] = Array($gramp[$n],$gramp[$n],$gramp[$n]);

$result = Array();
foreach($tab as $item)
{
  $index = ImageColorClosest($im, pow($item[0]/31,1.6)*255,
                                  pow($item[1]/31,1.6)*255,
                                  pow($item[2]/31,1.6)*255);
  $result[] = $index;
}

$res2 = Array();
for($n=0; $n<256; $n+=2)
  $res2[] = sprintf("0x%02X", $result[$n] + $result[$n+1]*16);

print "static const unsigned char Map256colors[256/2] = {" . join(',', $res2) . "};\n";
