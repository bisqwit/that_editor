<?php

if(1){
$slide_index = 1;
$slide = Array(6/*3*/,7,7,7,7,6/*3*/,6/*3*/,2,22);
}

if(0){
$slide_index = 2;
$slide = Array(248, /* somewhat white */
               15,  /* bright white */
               11,  /* bright yellow */
               186, /* semi dull yellow */
               136, /* dull yellow */
               8,   /* grey */
               0);
}

$im = ImageCreate(1,1);

function Alloc($r,$g,$b)
{
  global $im;
  #print "$r $g $b\t\t";
  $r = pow($r / 31, 2.0)*255;
  $g = pow($g / 31, 2.0)*255;
  $b = pow($b / 31, 2.0)*255;
  #print "$r $g $b\n";
  ImageColorAllocate($im, $r,$g,$b);
}

for($n=0; $n<16; ++$n)
{
  $r = ($n&8) ? (5 + ($n&1)/1 * 26) : (0 + ($n&1)/1 * 21);
  $g = ($n&8) ? (5 + ($n&2)/2 * 26) : (0 + ($n&2)/2 * 21);
  $b = ($n&8) ? (5 + ($n&4)/4 * 26) : (0 + ($n&4)/4 * 21);
  if($n==3) { $g = 5; }
  if($n==8) { $r=$g=$b = 7; }
  Alloc($r,$g,$b);
}
$gramp = Array(1,2,3,5,6,7,8,9,11,12,13,14,16,17,18,19,20,22,23,24,25,27,28,29);
$cramp = Array(0,12,16,21,26,31);
for($r=0; $r<6; ++$r) for($g=0; $g<6; ++$g) for($b=0; $b<6; ++$b)
  Alloc($cramp[$r],$cramp[$g],$cramp[$b]);
for($n=0; $n<24; ++$n) Alloc($gramp[$n],$gramp[$n],$gramp[$n]);

$lors = Array();
$poss = Array();

$prev = -1; $prev2 = -1;
$max = count($slide);
$slide[] = 0;
for($n=0; $n<65536; ++$n)
{
  $pos = $n*($max-1)/65536;
  $cur  = ImageColorsForIndex($im, $slide[floor($pos)]);
  $next = ImageColorsForIndex($im, $slide[ceil($pos)]);
  $shift = $pos - floor($pos);
  $index = ImageColorClosest($im, $next['red']*$shift + $cur['red']*(1-$shift),
                                  $next['green']*$shift + $cur['green']*(1-$shift),
                                  $next['blue']*$shift + $cur['blue']*(1-$shift));
  $kuu = floor($pos);
  if($kuu != $prev || $index != $prev2)
  {
    $prev = $kuu;
    $prev2 = $index;
    $lors[] = $index;
    $poss[] = $n;
  }
}
$n = count($poss);
foreach($poss as &$p) $p .= 'u'; unset($p);
print "static const unsigned char  slide{$slide_index}_colors[$n] = {" . join(',', $lors) . "};\n";
print "static const unsigned short slide{$slide_index}_positions[$n] = {" . join(',', $poss) . "};\n";

