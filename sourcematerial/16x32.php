<?php
/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */

$data = Array();
$mode = 0;

$matrix_row_size = (16 + 7) >> 3; // 2 bytes
$ascent = 0;
$descent = 0;

$bitmap = Array();
for($c=0; $c<256*32; ++$c) $bitmap[] = 0;

foreach(explode("\n",file_get_contents('16x32.bdf')) as $line)
{
  if(preg_match('/^FONT_ASCENT (.*)/', $line, $mat))
    $ascent = (int)$mat[1];
  elseif(preg_match('/^FONT_DESCENT (.*)/', $line, $mat))
    $descent = (int)$mat[1];
  elseif(preg_match('/^ENCODING (.*)/', $line, $mat))
    $chno = (int)$mat[1];
  elseif(preg_match('/^BBX (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)/', $line, $mat))
  {
    $x = (int) $mat[1];
    $y = (int) $mat[2];
    $xo = (int) $mat[3];
    $yo = (int) $mat[4];
    
    $shiftbits = ($matrix_row_size - (($x + 7) >> 3) ) * 8 - $xo;
    $beforebox = ($ascent - $yo - $y) * $matrix_row_size;
    $afterbox = ($descent + $yo) * $matrix_row_size;
  }
  elseif($line == 'BITMAP')
    $mode = 1;
  elseif($line == 'ENDCHAR')
  {
    $mode = 0;
    $map = Array();

    while($beforebox < 0)
      { array_shift($data); ++$beforebox; }
    while($afterbox < 0)
      { array_pop($data); ++$afterbox; }

    while($beforebox > 0)
      { $map[] = 0; --$beforebox; }
    foreach($data as $v)
      $map[] = $v;
    while($afterbox > 0)
      { $map[] = 0; --$afterbox; }

    if(0)
    {
      print "/* $chno */ \n";
      print_r($map);
    }
    elseif($chno < 128)
    {
      for($y=0; $y<32; ++$y)
      {
        $bitmap[$chno*32       +$y] = $map[$y] >> 8;
        $bitmap[($chno+0x80)*32+$y] = $map[$y] & 0xFF;
      }
    }
    $data = Array();
    ++$chno;
  }
  elseif($mode)
  {
    $v = intval($line, 16);
    if($shiftbits > 0)
      $v <<= $shiftbits;
    else
      $v >>= -$shiftbits;
    $data[] = $v;
  }
}

foreach($bitmap as $n => $v)
{
  printf('0x%02X,', $v);
  if( ($n%16)==15) print "\n";
}
