<?php

for($mode=0; $mode<12; ++$mode)
{
  $dim    = $mode & 1;  // 0,1
  $bold   = $mode & 2;
  $italic = $mode >> 2; // 0,1,2

  #if($dim)          ++$count;
  #if($bold)         ++$count;
  #if($italic)       ++$count;
  
  $calc = function($prev,$cur,$next)
  {
    global $dim,$bold,$italic;
    $result = $cur;
    if($dim  && $cur && !$next) $result = 1;
    if($bold && $prev && !$cur) $result = 1;
    return $result;
  };

  print "/*mode $mode*/";
  print "{";
  for($value=0; $value<16; ++$value)
  {
    $value0 = ($value & 8) ? 3 : 0; // before
    $value1 = ($value & 4) ? 3 : 0; // current
    $value2 = ($value & 2) ? 3 : 0; // after
    $value3 = ($value & 1) ? 3 : 0; // next
    
    $thisresult = $calc($value0,$value1,$value2);
    $nextresult = $calc($value1,$value2,$value3);

    if($italic == 1)
      $thisresult = (int)(($thisresult*2 + $nextresult*1)/3 + 0.5);
    if($italic == 2)
      $thisresult = (int)(($thisresult*1 + $nextresult*2)/3 + 0.5);
    print "$thisresult,";
  }
  print "},\n";
}
