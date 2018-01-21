<?php
$best_dist = 0xFFFFFFF;
$words = Array('black','blue','cyan','green','red','yellow','magenta','white',
               'BLACK','BLUE','CYAN','GREEN','RED','YELLOW','MAGENTA','WHITE',
               'bg_black','bg_blue','bg_cyan','bg_green','bg_red','bg_yellow','bg_magenta','bg_white',
               'BG_BLACK','BG_BLUE','BG_CYAN','BG_GREEN','BG_RED','BG_YELLOW','BG_MAGENTA','BG_WHITE',
               'underline','dim','italic','bold','inverse','blink');

for($n=0; $n<8; ++$n)
{
  $actions[$words[$n+0]] = sprintf("fg256=%2d", $n);
  $actions[$words[$n+8]] = sprintf("fg256=%2d", $n+8);
  $actions[$words[$n+16]] = sprintf("bg256=%2d", $n);
  $actions[$words[$n+24]] = sprintf("bg256=%2d", $n+8);
  if($n<6)
    $actions[$words[$n+32]] = sprintf("flags |= 0x%02X", 1 << ($n&7));
}

for($a=1; $a<6000; ++$a)
for($b=0; $b<256; ++$b)
for($mod=120; $mod>=30; --$mod)
for($div=1; $div<=16; ++$div)
{
  $data = Array();
  foreach($words as $s)
  {
    $c = 0;
    $l = strlen($s);
    $m = 0;
    for($n=0; $n<$l; ++$n)
    {
      $i = ord($s[$n]);
      $c = ($c + (($a^0)&0xFF)*$i + $m) & 0xFFFF;
      $m += $b;
    }
//Good: 9 25   distance = 72  mod=73

    $c = (int)($c / $div);
    $c %= $mod;
    $data[$c] = $s;
  }
  if(count($data) == count($words))
  {
    $dist = max(array_keys($data)) - min(array_keys($data));
    if($dist < $best_dist)
    {
      $best_dist = $dist;
      print "//Good: $a $b   distance = $best_dist  mod=$mod  div=$div\n";
      print "            while(*line && *line != ' ' && *line != '\t') { c += {$a}u*(unsigned char)*line + i; i+=$b; }\n";
      print "            switch((c / {$div}u) % {$mod}u\n";
      print "            {\n";
      //ksort($data);
      foreach($data as $c=>$s)
        printf("                case %2d: %s; break; // %s\n", $c, $actions[$s], $s);
      print "            }\n";
    }
  }
}