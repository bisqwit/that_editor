<?php

$iso_encodings = Array
(
  require 'iso_encodings.php.inc';
);
$cp_encodings = Array
(
  require 'cp_encodings.php.inc';
);

// PETSCII into CP437 conversion table
/*
    First 64:
    
    "@ A-Z [ pound ] uparrow leftarrow"
    space !"#$%&'()*+,-./
    0123456789:;<=>?
    
    **PETSCIITABLE UNSHIFTED60-CF**
...    
    Then 64:
    Special characters....
    
    Then 128, the same as before but in reverse.
    
    Then...
    
    "@ a-z [ pound ] uparrow leftarrow
    space !"#$%&'()*+,-./
    0123456789:;<=>?
    **PETSCIITABLE SHIFTED60-CF**
    horizline
    ABCDEFGHIJKLMNOPQRSTUVWXYZ pluscrossing
    
    
    
*/

$petscii = Array
(
    0xFF20,0xFF21,0xFF22,0xFF23,0xFF24,0xFF25,0xFF26,0xFF27, // 00: @A-Z...
    0xFF28,0xFF29,0xFF2A,0xFF2B,0xFF2C,0xFF2D,0xFF2E,0xFF2F,
    0xFF30,0xFF31,0xFF32,0xFF33,0xFF34,0xFF35,0xFF36,0xFF37, // 10
    0xFF38,0xFF39,0xFF3A,0xFF3B,0x20A4,0xFF3C,0x2191,0x2190,      // TODO: Duplicate A3, pick look-alike
    0x3000,0xFF01,0xFF02,0xFF03,0xFF04,0xFF05,0xFF06,0xFF07, // 20: punctuation...
    0xFF08,0xFF09,0xFF0A,0xFF0B,0xFF0C,0xFF0D,0xFF0E,0xFF0F,
    0xFF10,0xFF11,0xFF12,0xFF13,0xFF14,0xFF15,0xFF16,0xFF17, // 30: numbers...
    0xFF18,0xFF19,0xFF1A,0xFF1B,0xFF1C,0xFF1D,0xFF1E,0xFF1F,
    //Unshifted table 40..7F
    0x2500,0x2660,0x2502,0x2502,0x2599,0xFFFD,0xFFFD,0x259B, // 40
    0xFFFD,0x256E,0x2570,0x256F,0xFFFD,0x2572,0x2571,0xFFFD,
    0x259C,0x25CF,0xFFFD,0x2565,0xFFFD,0x256D,0x2573,0x25EF, // 50
    0x2563,0xFFFD,0x2566,0x253C,0xFFFD,0x2502,0x03C0,0x25E5,
    0xFFFD,0x258C,0x2584,0x2594,0x2581,0x258F,0x2592,0x2595, // 60
    0xFFFD,0x25E4,0x2595,0x251C,0x2597,0x2514,0x2510,0x2581,
    0x250C,0x2534,0x252C,0x2524,0x258F,0x258E,0x2595,0x2594, // 70
    0x2594,0x2582,0x259F,0x2596,0x259D,0x2518,0x2598,0x259A,
    // Then, the same as before, but in reverse. We encode them as enclosed alphanumerics...
    0x2182,0x24B6,0x24B7,0x24B8,0x24B9,0x24BA,0x24BB,0x24BC, // 80
    0x24BD,0x24BE,0x24BF,0x24C0,0x24C1,0x24C2,0x24C3,0x24C4,
    0x24C5,0x24C6,0x24C7,0x24C8,0x24C9,0x24CA,0x24CB,0x24CC, // 90
    0x24CD,0x24CE,0x24CF,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // A0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0x24EA,0x2460,0x2461,0x2462,0x2463,0x2464,0x2465,0x2466, // B0: numbers...
    0x2467,0x2468,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,

    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // C0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // D0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0x2588,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // E0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // F0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    // Now bank 2
    0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67, // 100
    0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77, // 110
    0x78,0x79,0x7A,0x5B,0xA3,0x5D,0x21D1,0x21D0,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27, // 120
    0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F, 
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, // 130
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    /////////////
    0x2015,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047, // 140
    0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057, // 150
    0x0058,0x0059,0x005A,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0x2593,
    0x25A0,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0x2591,0xFFFD, // 160
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 170
    0xFFFD,0xFFFD,0x2713,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,

    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 180
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 190
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 1A0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 1B0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 1C0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 1D0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 1E0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD, // 1F0
    0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
);

class Font
{
  public function GenerateOutput(
    // 8 bits per scanline, Height bytes per character
    // I.e. bytes for character N are N*height to (N+1)*height
    $bitmap,
    // Width and height
    $width, $height,
    $unicode_map)
  {
    $n = count($bitmap);
    print "static const unsigned char bitmap[$n] = {\n";
    $n=0;
    $chno=0;
    #print_r($unicode_map);
    $bytes_x = ($width+7) >> 3;
    foreach($bitmap as $value)
    {
      printf("0x%02X,", $value);
      if(++$n == $height * $bytes_x)
      {
        $n=0;
        printf(" /* %02X (U+%04X) */\n", $chno, $unicode_map[$chno]);
        #print "\n";
        ++$chno;
      }
    }
    print "};\n";
    
    $mask = 0xFFFF;
    if($chno <= 256) $mask = 0xFF;
    
    $revmap = Array();
    for($k=0; $k<0x10000; ++$k)
    {
      #$revmap[$k] = $k & $mask;
    }
    foreach($unicode_map as $k=>$v)
      $revmap[$v] = $k;

    GenerateOptimalTablePresentation($revmap, 'unicode_to_bitmap_index', $chno);
  }
  
  public function MakeUnicodeMap($encoding, $encodings)
  {
    global $iso_encodings, $cp_encodings, $petscii;
    $unicode_map = Array();
    $error = false;
    #print_r($encodings);
    foreach($encodings as $index => $value)
    {
      // TODO: Convert $value, which is in $encoding, into $unicode_value
      $unicode_value = 0;
      if($encoding == 'ISO10646-1')
        $unicode_value = $value;
      elseif(preg_match('/CP([0-9]+)/', $encoding, $mat))
        $unicode_value = $cp_encodings[ $mat[1] ] [ $value ];
      elseif(preg_match('/8859-([0-9]+)/', $encoding, $mat))
        $unicode_value = $iso_encodings[ $mat[1] ] [ $value ];
      elseif(preg_match('/PETSCII/', $encoding, $mat))
        $unicode_value = $petscii[ $value ];
      else
        $error = true;
      
      $unicode_map[$index] = $unicode_value;
    }

    #print "e($encoding)error($error)\n";
    #print_r($unicode_map);
    
    if($error)
      print "UNRECOGNIZED ENCODING: $encoding\n";

    return $unicode_map;
  }
};

function MakeCType($table, $size_only = false)
{
  $index = ceil( log(log(max($table)) / log(256)) / log(2) );
  if($index <= 0) $index = 0;
  if($size_only) return $index;

  $types = Array('uint_least8_t', 'uint_least16_t', 'uint_least32_t', 'uint_least64_t');
  return $types[$index];
}

function GenerateOptimalTablePresentation($table, $name, $significant)
{
  #$offset_map = Array();
  #foreach($table as $k=>$v)
  #  @$offset_map[$v-$k] += 1;
  #asort($offset_map);
  ksort($table);
  
  $tables = Array();

  $end = count($table);
  
  $default = 'index';

  if($significant <= 256)
  {
    // FIXME
    $default = "Help(index,InRecursion)";
  }
  
  #print_r($table);

  $mask = 0xFFFFFFF;
  if(max($table) < 65536) $mask = 0xFFFF;
  if(max($table) < 256)   $mask = 0xFF;

  // $table:   Unicode into INDEX
  
  $in_type  = MakeCType(array_keys($table));
  $out_type = MakeCType(array_values($table));

  $ifs = Array();

  /* First step: Find any sections that are long enough that consist
   *             of simply input_value + constant.
   */

  $prev_offset  = null;
  $region_begin = 0;
  $region_end   = 0;
  $good = Array();
  ksort($table);
  $flush = function()use(&$region_end,&$region_begin,&$good,&$prev_offset,$end)
  {
    $region_length = $region_end - $region_begin;
    if($region_length >= 8)
    {
      if($prev_offset != 0 
      || $region_begin == 0
      || $region_end   == $end)
      {
        $good[$prev_offset][] = Array($region_begin,$region_end);
      }
    }
  };
  foreach($table as $k=>$v)
  {
    $offset = $v - ($k & $mask);
    
    if($offset !== $prev_offset)
    {
      if($region_end > 0) $flush();
      $region_begin = $k;
      $prev_offset  = $offset;
    }
    $region_end = $k+1;
  }
  if($region_end > 0) $flush();

  foreach($good as $offset => $regions)
  {
    $rstr = Array();
    $coverage = 0;
    $moffset = -$offset;
    foreach($regions as $r)
      $ifs[ $r[0] ]
        = Array( $r[0], $r[1], "index - $moffset"
               );
  }

  foreach($good as $datas)
    foreach($datas as $data)
      for($k=$data[0]; $k!=$data[1]; ++$k)
        unset($table[$k]);

  /* After this, remove any part where key==value */
  foreach($table as $k=>$v)
    if($v == ($k & $mask))
      unset($table[$k]);

  /* Second step: Find any sections that are long enough that
   *              consist of _any_ values for consecutive keys.
   */
  
  $region_begin = 0;
  $region_end   = -1;
  $good = Array();
  asort($table);
  $flush = function()use(&$region_end,&$region_begin,&$good,$end)
  {
    $region_length = $region_end - $region_begin;
    if($region_length >= 8)
    {
      $good[$region_begin] = $region_end;
    }
  };
  #print_r($table);
  foreach($table as $k=>$v)
  {
    if($k != $region_end)
    {
      if($region_end > 0) $flush();
      $region_begin = $k;
    }
    $region_end = $k+1;
  }
  if($region_end > 0) $flush();
  #print_r($good);

  foreach($good as $begin => $end)
  {
    $tab = Array();
    for($k=$begin; $k!=$end; ++$k)
      $tab[] = $table[$k];
    
    $tabname = "{$name}_" . MakeCType($tab,true);
    $tabbegin = count(@$tables[$tabname]['data']);
    foreach($tab as $k)
      $tables[$tabname]['data'][] = $k;
    $tables[$tabname]['type'] = MakeCType($tab);

    $offs = $tabbegin - $begin;
    $ifs[$begin]
      = Array( $begin, $end, "{$tabname}[index + $offs]"
             );
  }

  foreach($good as $begin=>$end)
    for($k=$begin; $k!=$end; ++$k)
      unset( $table[$k] );


if(0)
{
  /* Third step: Find any sections that are long enough that
   *             consist of _any_ values for consecutive VALUES.
   */
  
  $region      = Array();
  $prev_value  = null;
  $first_value = null;

  $good = Array();
  asort($table);
  $flush = function()use(&$region,&$first_value,&$prev_value,&$good,$end)
  {
    if(count($region) >= 8)
    {
      $good[$first_value] = $region;
    }
  };
  #print_r($table);
  foreach($table as $k=>$v)
  {
    if(($v-1) !== $prev_value)
    {
      $flush();
      $region      = Array();
      $first_value = $v;
    }
    $region[]   = $k;
    $prev_value = $v;
  }
  $flush();
  #print_r($good);

  foreach($good as $first_value => $tab)
  {
    $begin = min($tab);
    $end   = max($tab)+1;
    if(count($tab) == 1)
    {
      $ifs[$begin]
        = Array( $begin, $end, "$first_value"
               );
    }
    else
    {
      $tabname = "{$name}_" . MakeCType($tab,true);
      $tabbegin = count(@$tables[$tabname]['data']);
      foreach($tab as $k)
        $tables[$tabname]['data'][] = $k;
      $tabend   = count(@$tables[$tabname]['data']);
      $tables[$tabname]['type'] = MakeCType($tab);

      $ifs[$begin]
        = Array( $begin, $end, "binarysearch(index, $tabname+$tabbegin, $tabname+$tabend, [](unsigned pos)->$out_type { return $first_value + pos; },".
                                                     " $default)"
               );
    }
  }

  foreach($good as $first_value => $tab)
    foreach($tab as $k)
      unset( $table[$k] );
}//third part endif

  asort($table);

  $n = count($table);
  if($n)
  {
    $tab = array_keys($table);
    $intabname = "{$name}_" . MakeCType($tab,true);
    $intabbegin = count(@$tables[$intabname]['data']);
    foreach($tab as $k)
      $tables[$intabname]['data'][] = $k;
    $intabend   = count(@$tables[$intabname]['data']);
    $tables[$intabname]['type'] = MakeCType($tab);

    $tab = array_values($table);
    $outtabname = "{$name}_" . MakeCType($tab,true);
    $outtabbegin = count(@$tables[$outtabname]['data']);
    foreach($tab as $k)
      $tables[$outtabname]['data'][] = $k;
    $outtabend   = count(@$tables[$outtabname]['data']);
    $tables[$outtabname]['type'] = MakeCType($tab);
  }

  foreach($tables as $tabname => $tabdata)
  {
    $tabsize = count($tabdata['data']);
    print "static const {$tabdata['type']} {$tabname}[$tabsize] =\n";
    print "{ " . join(',', $tabdata['data']) . " };\n";
  }
  
  print "static const struct {$name}_generator\n";
  print "{\n";
  if($n)
  {
    print "    template<typename T, typename T2>\n";
    print "    static $out_type binarysearch\n";
    print "       ($in_type index, const T* begin, const T* end,\n";
    print "        T2&& act, $out_type other)\n";
    print "    {\n";
    print "        auto i = std::lower_bound(begin, end, index);\n";
    print "        return (i != end && *i == index) ?  act(i-begin) : other;\n";
    print "    }\n";
    print "    static $out_type DefFind($in_type index)\n";
    print "    {\n";

    print "        return binarysearch(index, $intabname+$intabbegin, $intabname+$intabend,\n";
    print "             [](unsigned pos)->$out_type { return {$outtabname}[$outtabbegin+pos]; },\n";
    print "             $default)\n";
    print "    }\n";
  }
  
  $other = $n ? "DefFind(index)" : "$default";

  if($significant <= 256)
  {
    print "    static $out_type Help($in_type index, bool InRecursion)\n";
    print "    {\n";
    print "        return InRecursion ? (index & 0xFF) : Find(UnicodeToASCIIapproximation(index)&0xFF, true)\n";
    print "    }\n";
  }
  print "    static $out_type Find($in_type index, bool InRecursion)\n";
  print "    {\n";
  print "        return\n";

  ksort($ifs);
  #print_r($ifs);
  MakeRecursiveIftree($ifs, $other, '        ', 0,0x1FFFFF);
 
  print "        ;\n";
  print "    }\n";

  print "    $out_type operator[] ($in_type index) const { return Find(index, false); }\n";
  print "} $name;\n";
}

function MakeRecursiveIfTree($ifs, $other, $indent, $ge,$lt, $first='  ')
{
  /* $ge,$lt describes the confidence of what we know currently.
  */

  // First, figure out which conditions _can_ apply here.
  $applicable = Array();
  foreach($ifs as $begin=>$case)
    if($begin < $lt && $case[1] > $ge)
      $applicable[$begin] = $case;

  switch(count($applicable))
  {
    case 0:
      print "{$indent}{$first}$other\n";
      return;
    case 1:
      foreach($applicable as $begin=>$case) {}
      $expression = $case[2];
      $conditions = Array();
      if($case[0] > $ge) $conditions[] = "index >= {$case[0]}";
      if($case[1] < $lt) $conditions[] = "index <  {$case[1]}";
      if(!empty($conditions))
        $expression = "((".join(" && ",$conditions).") ? $expression : $other)";
      print "{$indent}{$first}$expression\n";
      return;
    case 2:
      reset($applicable);
      list($dummy,$a) = each($applicable);
      list($dummy,$b) = each($applicable);
      reset($applicable);
      #print "-- $ge, $lt --\n";
      #print serialize($a)."\n";
      #print serialize($b)."\n";
      if($b[0] == $a[1] && $a[0] == $ge && $b[1] == $lt)
      {
        $expression = "((index < {$b[0]}) ? {$a[2]} : {$b[2]})";
        print "{$indent}{$first}$expression\n";
        return;
      }
  }
  // Find midpoint from the selection.
  $n = count($applicable);
  $m = 0;
  $midpoint = 0;
  foreach($applicable as $begin=>$case)
  {
    if($m >= $n) { $midpoint = $begin; break; }
    $m += 2;
  }

  print "{$indent}{$first}(index < $midpoint)\n";
  MakeRecursiveIfTree($ifs, $other, "$indent  ", $ge, $midpoint, '? ');
  MakeRecursiveIfTree($ifs, $other, "$indent  ", $midpoint, $lt, ': ');
}
