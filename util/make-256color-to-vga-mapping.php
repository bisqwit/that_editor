<?php

$im    = null;
$gamma = 1;

function Alloc($r,$g,$b)
{
  global $im, $gamma;
  #print "$r $g $b\t\t";
  $r = pow($r / 31, $gamma)*255;
  $g = pow($g / 31, $gamma)*255;
  $b = pow($b / 31, $gamma)*255;
  #print "$r $g $b\n";
  ImageColorAllocate($im, $r,$g,$b);
}

$tables = Array();
for($tengamma=17; $tengamma <= 18; ++$tengamma)
{
  $gamma = $tengamma/10;
  $im = ImageCreate(1,1);

  $tab = Array();
  for($n=0; $n<16; ++$n)
  {
    $r = ($n&8) ? (5 + ($n&1)/1 * 26) : (0 + ($n&1)/1 * 21);
    $g = ($n&8) ? (5 + ($n&2)/2 * 26) : (0 + ($n&2)/2 * 21);
    $b = ($n&8) ? (5 + ($n&4)/4 * 26) : (0 + ($n&4)/4 * 21);
    if($n==3) { $g       = 5; }
    if($n==8) { $r=$g=$b = 7; }
    $tab[] = Array($b,$g,$r);
    Alloc($b,$g,$r);
  }

  $gramp = Array(1,2,3,5,6,7,8,9,11,12,13,14,16,17,18,19,20,22,23,24,25,27,28,29);
  $cramp = Array(0,12,16,21,26,31);
  for($r=0; $r<6; ++$r) for($g=0; $g<6; ++$g) for($b=0; $b<6; ++$b)
    $tab[] = Array($cramp[$r],$cramp[$g],$cramp[$b]);
  for($n=0; $n<24; ++$n) $tab[] = Array($gramp[$n],$gramp[$n],$gramp[$n]);

  $result = &$tables[$tengamma];
  $result = Array();
  foreach($tab as $item)
  {
    $index = ImageColorClosest($im, pow($item[0]/31,$gamma)*255,
                                    pow($item[1]/31,$gamma)*255,
                                    pow($item[2]/31,$gamma)*255);
    $result[] = $index;
  }
  unset($result);
}
$result = $tables[1.8 * 10];

$res2 = Array();
for($n=0; $n<256; $n+=2)
  $res2[] = sprintf("0x%02X", $result[$n] + $result[$n+1]*16);

print "static const unsigned char Map256colors[256/2] = {" . join(',', $res2) . "};\n";

$fp = fopen('/dev/tty', 'w');
if($fp)
{
  $fgkw = Array('black','blue','green','cyan','red','yellow','magenta','white','BLACK','BLUE','GREEN','CYAN','RED','YELLOW','MAGENTA','WHITE');
  $bgkw = Array('bg_black','bg_blue','bg_cyan','bg_red','bg_yellow','bg_magenta','bg_white','BG_BLACK','BG_BLUE','BG_CYAN','BG_GREEN','BG_RED','BG_YELLOW','BG_MAGENTA','BG_WHITE');
  foreach(explode("\n", file_get_contents('c.jsf')) as $line)
    if(preg_match('/^=([^ 	]*)[ 	]*(.*?) *$/', $line, $mat))
    {
      $name = $mat[1];
      $fg = 7;
      $bg = 0;
      $attr = 0;
      preg_match_all('/([^ 	]+)/', $mat[2], $mat);
      foreach($mat[0] as $w)
      {
        if($w=='underline') $attr |= 0x01;
        if($w=='dim') $attr |= 0x02;
        if($w=='italic') $attr |= 0x04;
        if($w=='bold') $attr |= 0x08;
        if($w=='inverse') $attr |= 0x10;
        if($w=='blink') $attr |= 0x20;
        if(($k = array_search($w, $fgkw)) !== false) $fg = $k;
        if(($k = array_search($w, $bgkw)) !== false) $bg = $k;
        if(preg_match('/fg_([0-5][0-5][0-5])/', $w, $ma)) $fg = 16+intval($ma[1], 6);
        if(preg_match('/bg_([0-5][0-5][0-5])/', $w, $ma)) $bg = 16+intval($ma[1], 6);
      }
      
      $code = "\33[0;38;5;$fg;48;5;$bg";
      if($attr&1) $code .= ";4";
      if($attr&2) $code .= ";2";
      if($attr&4) $code .= ";3";
      if($attr&8) $code .= ";1";
      if($attr&16) $code .= ";7";
      if($attr&32) $code .= ";5";
      
      $code .= 'm';
      $reset = "\33[m";
      
      fprintf($fp, "%-24s: %sORIGINAL%s", $name, $code, $reset);
      
      foreach($tables as $tengamma => $result)
      {
        $nfg = $result[$fg];  $nfg = ($nfg&10) | (($nfg&4)>>2) | (($nfg&1)<<2);
        $nbg = $result[$bg];  $nbg = ($nbg&10) | (($nbg&4)>>2) | (($nbg&1)<<2);
        $newcode = "\33[0;38;5;$nfg;48;5;$nbg";
        $newcode .= 'm';
        fprintf($fp, " %s%.1f%s", $newcode, $tengamma/10, $reset);
      }
      fprintf($fp, "\n");
    }
  
}

