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

$registry = 'C64';
$encoding = 'PETSCII';
$encodings = Array();
$fontwidth  = 8;
$fontheight = 8;

$bitmap = Array();

$data = file_get_contents($fontfile);
for($n=0; $n<0x1000; ++$n)
  $bitmap[$n] = ord($data[$n]);
$encodings = range(0,511);

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
