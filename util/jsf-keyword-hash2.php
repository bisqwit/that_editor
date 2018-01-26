<?php
$words = Array("noeat\0","buffer\0","markend\0","mark\0","strings\0","istrings\0","recolormark\0","recolor=");
$bestrange = 9999999;
$bestsum   = 9999999;

for($down=0; $down<4; ++$down)
for($delta=0; $delta < (1 << $down); ++$delta)
for($shift=0; $shift<32; ++$shift)
for($start=0; $start<=$shift; ++$start)
for($bits=1; $bits<4; ++$bits)
{
  $mask = (1 << $bits)-1;
  if($shift > $mask) continue;
  for($offs = 0; $offs <= $mask; ++$offs)
  {
    $wrd = Array();
    foreach($words as $s)
    {
      $n = $delta;
      $v = $start;
      $b = strlen($s);
      for($a=0; $a<$b; ++$a)
      {
        $v = $v & 0xFF;
        $n += (((ord($s[$a]) ^ $v) + $offs));
        $v += $shift;
        $n = $n & 0xFF;
      }
      $n >>= $down;
      $n &= $mask;
      
      if(isset($wrd[$n]))
      {
        continue 2;
      }
      $wrd[$n] = $s;
    }
    $mi = min(array_keys($wrd));
    $ma = max(array_keys($wrd));
    $range = $ma-$mi;

    #$sum = max($shift,$offs,$delta) * 16 + ($shift+$offs+$delta);
    $sum = $shift*$shift + $offs*$offs + $delta*$delta + $start*$start;
    
    if($range < $bestrange
    || ($range == $bestrange && $sum <= $bestsum)
      )
    {
      $bestrange = $range;
      $bestsum   = $sum;
      print "range $range offs=$offs shift=$shift mask=$mask down=$down delta=$delta start=$start\n";
      ksort($wrd);
      print_r($wrd);
      
      print "n=$delta; v=$start;\n";
      print "  n += (c ^ v) + $offs;\n";
      print "  v += $shift;\n";
      print "switch(((n) >> $down) & $mask)\n";
      
    }
  }
}
