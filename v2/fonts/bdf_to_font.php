<?php

/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
print "/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */\n";

require 'font_gen.php';

$fontfile   = null;
$outputfile = null;
$globalname = null;

$options = getopt('f:o:n:', Array('font:','output:','name:'));
foreach($options as $k=>$v)
{
  if($k=='f' || $k=='font')   $fontfile = $v;
  if($k=='o' || $k=='output') $outputfile = $v;
  if($k=='n' || $k=='name')   $globalname = $v;
}

$chno = 0;
$data = Array();
$mode = 0;

$matrix_row_size = (8 + 7) >> 3; // 1 byte
$ascent = 0;
$descent = 0;

$registry = '';
$encoding = '';
$encodings = Array();
$fontwidth  = 8;
$fontheight = 1;

$bitmap = Array();

foreach(explode("\n",file_get_contents($fontfile)) as $line)
{
  if(preg_match('/^FONT_ASCENT (.*)/', $line, $mat))
    $ascent = (int)$mat[1];
  elseif(preg_match('/^FONT_DESCENT (.*)/', $line, $mat))
    $descent = (int)$mat[1];
  elseif(preg_match('/^ENCODING (.*)/', $line, $mat))
    $encodings[$chno] = (int)$mat[1];
  elseif(preg_match('/^FONT -.*-([^-]*)-([^-]*)$/', $line, $mat))
  {
    $registry = $mat[1];
    $encoding = $mat[2];
  }
  elseif(preg_match('/^CHARSET_REGISTRY "?([^"]*)/', $line, $mat))
    $registry = $mat[1];
  elseif(preg_match('/^CHARSET_ENCODING "?([^"]*)/', $line, $mat))
    $encoding = $mat[1];
  elseif(preg_match('/^FONTBOUNDINGBOX ([0-9]*) ([0-9]*)/', $line, $mat))
  {
    $fontwidth  = (int)$mat[1];
    $fontheight = (int)$mat[2];
    $matrix_row_size = ($fontwidth + 7) >> 3; // 1 byte
  }
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

    if($fontwidth <= 8 || $chno < 0x80)
      for($y=0; $y<$fontheight; ++$y)
      {
        $m = (int)@$map[$y];
        if($fontwidth > 8)
        {
          $bitmap[($chno+0x00)*$fontheight + $y] = $m >> 8;
          $bitmap[($chno+0x80)*$fontheight + $y] = $m & 0xFF;
        }
        else
        {
          $bitmap[$chno*$fontheight + $y] = $m;
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

for($n=0; $n<256*$fontheight*ceil($fontwidth/8); ++$n)
  if(!isset($bitmap[$n]))
    $bitmap[$n] = 0;
ksort($bitmap);



ob_start();

print "namespace ns_$globalname {\n";

$font = new Font;
$enc = trim($registry.'-'.$encoding, "- \t\r\n");
$font->GenerateOutput($bitmap, $fontwidth, $fontheight,
                      $font->MakeUnicodeMap($enc, $encodings)
                     );

print "}\n";
print "struct $globalname: public UIfontBase\n";
print "{\n";
print "    virtual const unsigned char* GetBitmap() const { return ns_$globalname::bitmap; }\n";
print "    virtual unsigned GetIndex(char32_t c) const { return ns_$globalname::unicode_to_bitmap_index[c]; }\n";
print "};\n";


file_put_contents($outputfile, ob_get_clean());
