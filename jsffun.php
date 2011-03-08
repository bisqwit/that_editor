<?php

define('JSF_MAXBUF', 20);

class JSF_Parser
{
  private $name;
  public $color;
  private $now;
  public $state;
  public $styles;

  public $fg_default;
  public $bg_default;
  private $default_color_set;

  function __construct($syntax)
  {
    $this->name = $syntax;
    $this->color = Array();
    $this->state = Array();
    $this->now   = '';
    $this->fg_default = 0;
    $this->bg_default = 15;
    $this->default_color_set = false;
  }

  public function save()
  {
    return Array('state'  => $this->state,
                 'color'  => $this->color,
                 'styles' => $this->styles,
                 'bg_default' => $this->bg_default,
                 'fg_default' => $this->fg_default);
  }
  public function load($obj)
  {
    $this->color = $obj['color'];
    $this->state = $obj['state'];
    $this->styles= $obj['styles'];
    $this->bg_default = $obj['bg_default'];
    $this->fg_default = $obj['fg_default'];
  }

  private function ParseColorDecl($line)
  {
    $tok = preg_split("/[ \t]+/", $line);

    $fg    = $this->fg_default;
    $bg    = $this->bg_default;

    $under = false;
    $inv   = false;
    $bold  = 400;
    $italic= false;
    $blink = false;
    $name = '';
    foreach($tok as $tok)
    {
      if(!$name) $name = $tok;
      else switch($tok)
      {
        case 'bold':      $bold = 700; break;
        case 'semibold':  $bold = 600; break;
        case 'italic':$italic=true; break;
        case 'blink': $blink=true; break;
        case 'inverse': $inv = true; break;
        case 'underline': $under = true; break;
        case 'black':   $fg &= ~15; $fg |= 0; break;
        case 'blue':    $fg &= ~15; $fg |= 1; break;
        case 'green':   $fg &= ~15; $fg |= 2; break;
        case 'cyan':    $fg &= ~15; $fg |= 3; break;
        case 'red':case 'reg':
                        $fg &= ~15; $fg |= 4; break;
        case 'magenta': $fg &= ~15; $fg |= 5; break;
        case 'yellow':  $fg &= ~15; $fg |= 6; break;
        case 'white':   $fg &= ~15; $fg |= 7; break;
        case 'bg_black':   $bg &= ~15; $bg |= 0; break;
        case 'bg_blue':    $bg &= ~15; $bg |= 1; break;
        case 'bg_green':   $bg &= ~15; $bg |= 2; break;
        case 'bg_cyan':    $bg &= ~15; $bg |= 3; break;
        case 'bg_red':case 'bg_reg':
                           $bg &= ~15; $bg |= 4; break;
        case 'bg_magenta': $bg &= ~15; $bg |= 5; break;
        case 'bg_yellow':  $bg &= ~15; $bg |= 6; break;
        case 'bg_white':   $bg &= ~15; $bg |= 7; break;
        case 'BLACK':   $fg &= ~15; $fg |= 8+0; break;
        case 'BLUE':    $fg &= ~15; $fg |= 8+1; break;
        case 'GREEN':   $fg &= ~15; $fg |= 8+2; break;
        case 'CYAN':    $fg &= ~15; $fg |= 8+3; break;
        case 'RED':case 'REG':
                        $fg &= ~15; $fg |= 8+4; break;
        case 'MAGENTA': $fg &= ~15; $fg |= 8+5; break;
        case 'YELLOW':  $fg &= ~15; $fg |= 8+6; break;
        case 'WHITE':   $fg &= ~15; $fg |= 8+7; break;
        case 'BG_BLACK':   $bg &= ~15; $bg |= 8+0; break;
        case 'BG_BLUE':    $bg &= ~15; $bg |= 8+1; break;
        case 'BG_GREEN':   $bg &= ~15; $bg |= 8+2; break;
        case 'BG_CYAN':    $bg &= ~15; $bg |= 8+3; break;
        case 'BG_RED':case 'BG_REG':
                           $bg &= ~15; $bg |= 8+4; break;
        case 'BG_MAGENTA': $bg &= ~15; $bg |= 8+5; break;
        case 'BG_YELLOW':  $bg &= ~15; $bg |= 8+6; break;
        case 'BG_WHITE':   $bg &= ~15; $bg |= 8+7; break;
        default:
        {
          if(preg_match('@^bg_[0-5][0-5][0-5]@', $tok))
            $bg = 10000 + (int)substr($tok, 3);
          if(preg_match('@^fg_[0-5][0-5][0-5]@', $tok))
            $fg = 10000 + (int)substr($tok, 3);
        }
      }
    }

    $this->color[$name] =
     Array
     (
       'fg' => $fg,
       'bg' => $bg,
       'under' => $under,
       'inv'   => $inv,
       'bold'  => $bold,
       'blink' => $blink,
       'italic'=> $italic,
       'name' => $name
     );

    if(!$this->default_color_set)
    {
      $this->default_color_set = true;
      $this->fg_default = $fg;
      $this->bg_default = $bg;
    }
  }

  private function ParseStateStart($line)
  {
    $tok = preg_split("/[ \t]+/", $line);
    $name  = $tok[0];
    $state = $tok[1];

    unset($this->state[$this->now]['cur_table']);

    $this->state[$name] =
     Array
     (
       'name'      => $name,
       'color'     => $state,
       'cur_table' => false,
       'options'   => Array()
     );
    $this->now = $name;
  }

  private function ParseStateLine($tab)
  {
    $state = &$this->state[$this->now];

    $is_table = $state['cur_table'] !== false;

    #print "{this->now}: $is_table {$state['cur_table']} ".json_encode($tab)."\n";
    #print_r($state['options']);

    if($is_table)
    {
      $tableno = $state['cur_table'];

      if($tab == Array('done'))
      {
        $state['cur_table'] = false;
      }
      else
      {
        $table = &$state['options'][$tableno]['table'];

        $s = $tab[0];

        $s = preg_replace('@^"(.*)"$@', '\1', $s);
        $s = str_replace(Array('\n', '\r', '\t', '\\"'),
                         Array("\n", "\r", "\t", '"'), $s);
        $s = preg_replace('@\\\\([^\\\\])@', '\1', $s);
        $next_state = $tab[1];

        if($state['options'][$tableno]['strings'] == 2)
        {
          $s = strtoupper($s);
        }

        $table[$s] = $next_state;
      }
    }
    else
    {
      $regexp = false;
      $new_state = '';
      $noeat  = false;
      $buffer = false;
      $recolor= 0;
      $strings= '';
      foreach($tab as $item)
      {
        if($regexp === false)
        {
          $regexp = '';
          if($item != '*')
          {
            $s = $item;
            #print "in($item)";
            $s = preg_replace('@^"(.*)"$@', '\1', $s);
            $s = str_replace(Array('\n', '\r', '\t', '\"'),
                             Array("\n", "\r", "\t", '"'), $s);
            $s = preg_replace('@\\\\([^\\\\-])@', '\1', $s);
            $s = str_replace(']', '\]', $s);

            #print "out($s)\n";
            if(preg_match('@^\^@', $s)) $s = '\\'.$s;
            $regexp = '^['.$s.']$';
          }
          continue;
        }
        if(!$new_state)
        {
          $new_state = $item;
        }
        elseif($item == 'noeat')  $noeat = true;
        elseif($item == 'buffer') $buffer = true;
        elseif(preg_match('@^recolor=@', $item)) $recolor = (int)substr($item, 8);
        elseif($item == 'strings') $strings = 1;
        elseif($item == 'istrings') $strings = 2;
        else
        {
          // huh?
        }
      }
      if($regexp == '')
        $allowed_chars = true;
      else
      {
        $allowed_chars = Array();
        for($c=0; $c<256; ++$c)
          if(preg_match("$regexp", chr($c)))
            $allowed_chars[$c] = chr($c);
      }

      $choice = Array
      (
        'regexp'   => $regexp,
        'state'    => $new_state,
        'noeat'    => $noeat,
        'buffer'   => $buffer,
        'recolor'  => $recolor,
        'strings'  => $strings,
        'table'    => Array(),
        'allowed'  => $allowed_chars
      );
      $state['options'][] = $choice;

      if($strings)
      {
        $state['cur_table'] = count($state['options'])-1;
      }
    }
  }

  public static function EGAcolor($color)
  {
    if($color >= 10000)
    {
      $r = (int)($color/100)%10;
      $g = (int)($color/10)%10;
      $b = (int)($color/1)%10;
      return sprintf('#%X%X%X', $r*15/5, $g*15/5, $b*15/5);
    }

    static $EGA_COLOR_MAP = Array
    (
      '#000', '#33A', '#1A1', '#1AA',
      '#A11', '#A1A', '#A51', '#AAA',
      '#555', '#77F', '#5F5', '#5FF',
      '#F55', '#F5F', '#FF5', '#FFF'
    );

    return $EGA_COLOR_MAP[$color];
  }

  private function GenStyles()
  {
    $this->styles = Array();
    foreach($this->color as $name => &$color)
    {
      $defs = Array();
      /*
      ob_start();
       print_r($color);
       $tmp = ob_get_contents();
      ob_end_clean();
      $defs[] = "\n/"."* $tmp *"."/\n";
      */

      $defs[] = 'background:'.$this->EGAcolor($color[$color['inv']?'fg':'bg']);
      $defs[] = 'color:'.$this->EGAcolor($color[$color['inv']?'bg':'fg']);
      if($color['under']) $defs[] = 'text-decoration:underline';
      if($color['bold'] != 400) $defs[] = "font-weight:{$color['bold']}";
      if($color['italic']) $defs[] = 'font-style:italic';
      $style = '{'.join(';', $defs).'}';

      $color['style'] = $style;

      $name = '.' . $this->EncodeStyleName($name);
      $this->styles[$style][$name] = $name;
    }
  }

  public function GetBackgroundColorInCSSformat()
  {
    return
      'background-color:'.$this->EGAcolor($this->bg_default).
     ';color:'.$this->EGAcolor($this->fg_default);
  }

  public function GetStyles()
  {
    $styletext = '';
    foreach($this->styles as $style => $names)
      $styletext .= join(',', $names) . $style . "\n";
    return $styletext;
  }

  public function ParseSyntax($jsf_lines)
  {
    foreach($jsf_lines as $line)
    {
      $line = preg_replace("@[\r\n]+\$@", '', $line);
      $line = preg_replace('@^((?:"(?:\\\\.|[^"])"|[^#"])*)(#.*)?@', '\1', $line);
      $line = preg_replace("@[ \t]*\$@", '', $line);
      #print "$line\n";
      if(preg_match('@^=@', $line))
      {
        $this->ParseColorDecl(substr($line, 1));
      }
      elseif(preg_match('@^:@', $line))
      {
        $this->ParseStateStart(substr($line, 1));
      }
      elseif(preg_match("@^[ \t]@", $line) || $line == 'done')
      {
        #print "$line\n";
        preg_match_all('@"(?:\\\\"|[^"])+"|\*|[-=\w]+@', $line, $tab);
        $tab = $tab[0];
        #print_r($tab);
        $this->ParseStateLine($tab);
      }
    }
    $prev_state = &$this->state[$this->now];
    unset($prev_state['cur_table']);

    $this->GenStyles();
  }

  public function EncodeStyleName($name)
  {
    //return "jsf_{$this->name}_$name";
    //print_r($this->color[$name]);
    //$int = crc32($this->name.'.'.$name);
    $int = crc32($this->color[$name]['style']);

    $c="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-"/*
       "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ".
       "ĞÑÒÓÔÕÖØÙÚÛÜİŞß".
       "àáâãäåæçèéêëìíîï".
       "ğñòóôõöøùúûüışÿ"*/;

    $res='jSf';
    $n=strlen($c);
    if($int<0)$int=-$int;
    while($int>0)
    {
      $res .= $c[$int%$n];
      $int = (int)($int/$n);
    }
    return /*utf8_encode*/($res);
  }
};

