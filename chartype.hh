#if defined(__BORLANDC__) || defined(USE_DOSCOLORS)
/* Use 16-bit element types (VGA colors) in the 16-bit code
 * in order to avoid running out of memory
 */
# define ATTRIBUTE_CODES_IN_VGA_ORDER
#else
/* Use 32-bit element types (xterm-256color) in the 32-bit code,
 * requires a patched DOSBox though
 */
# define ATTRIBUTE_CODES_IN_ANSI_ORDER
#endif


#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
  #include "vec_lp.hh"
  typedef LongPtrVecType  EditorLineVecType;
  typedef LongVecType     EditorCharVecType;
  typedef unsigned long   EditorCharType;
#endif
#ifdef ATTRIBUTE_CODES_IN_VGA_ORDER
  #include "vec_sp.hh"
  typedef WordPtrVecType  EditorLineVecType;
  typedef WordVecType     EditorCharVecType;
  typedef unsigned short  EditorCharType;
#endif

static inline EditorCharType ExtractColor(EditorCharType ch)
{
    return ch & ~EditorCharType(0xFF);
}

static inline unsigned char ExtractCharCode(EditorCharType ch)
{
    return ch;// & 0xFFu;
}

static inline EditorCharType Recolor(EditorCharType ch, EditorCharType attr)
{
    return ExtractCharCode(ch) | ExtractColor(attr);
}

#ifdef ATTRIBUTE_CODES_IN_VGA_ORDER
static inline unsigned Closest256attribute(unsigned c)
{
static const unsigned char Map256colors[256/2] = {0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE,0x80,0x11,0x91,0x88,0x33,0x93,0x22,0x33,0x93,0x22,0x32,0x93,0x22,0xA2,0xBB,0xAA,0xAA,0xBB,0x88,0x11,0x91,0x88,0x38,0x93,0x82,0x38,0x93,0x22,0x72,0x97,0xA2,0x7A,0xBB,0xAA,0xAA,0xBB,0x44,0x55,0x95,0x84,0x55,0x95,0x66,0x77,0x97,0x66,0x77,0x97,0x66,0x77,0xB7,0xAA,0xAA,0xBB,0x44,0x55,0x95,0x44,0x55,0x95,0x66,0x77,0x97,0x66,0x77,0x77,0x66,0x77,0xB7,0xAA,0x7A,0xBB,0x44,0x55,0xD5,0xC4,0x55,0xD5,0x66,0x77,0xD7,0x66,0x77,0xD7,0x66,0x77,0xF7,0xEE,0xEE,0xFF,0xCC,0xCC,0xDD,0xCC,0xCC,0xDD,0xCC,0xCC,0xDD,0xCC,0x7C,0xDD,0xEE,0xEE,0xFF,0xEE,0xEE,0xFF,0x00,0x80,0x88,0x88,0x88,0x88,0x77,0x77,0x77,0x77,0xF7,0xFF};
    if(c&1) return Map256colors[c>>1] >> 4;
    return Map256colors[c>>1] & 0xF;
}
#endif

static EditorCharType ComposeEditorChar(
    unsigned char ch,
    unsigned char fg_color_index,
    unsigned char bg_color_index,
    unsigned char flags=0)
{
    if(flags & 0x10) // Deal with inverse-flag
    {
        unsigned tmp=fg_color_index; fg_color_index=bg_color_index; bg_color_index=tmp;
        flags &= ~0x10;
    }

#ifdef ATTRIBUTE_CODES_IN_VGA_ORDER
    EditorCharType result = ch;
    // Create a 8-bit CGA/EGA/VGA attribute
    result |= Closest256attribute(fg_color_index) << 8;
    result |= Closest256attribute(bg_color_index) << 12;
    result |= (flags & 0x20) <<10; // blink
    result |= (flags & 0x08) << 8; // high-intens
    return result;
#endif
#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
    EditorCharType result = ch;
  #if 0
    if(fg_color_index < 16 && bg_color_index < 16)
    {
        // Create a 8-bit CGA/EGA/VGA attribute
        result |= fg_color_index << 8;
        result |= bg_color_index << 12;
        result |= (flags & 0x20) <<10; // blink
        result |= (flags & 0x08) << 8; // high-intens
    }
    else
  #endif
    {
        // Create an extended attribute
        flags |= 0x80 | ((fg_color_index & 0x80) >> 1);
        unsigned high = bg_color_index | (flags << 8);
        unsigned low  = ((fg_color_index | 0x80) << 8);
        result |= low;
        result |= ((unsigned long)high) << 16;
    }
    return result;
#endif
}

static inline EditorCharType MakeDefaultColor(unsigned char ch)
{
    return ch | 0x0700;//ComposeEditorChar('\0', 7,0);
}

static inline EditorCharType MakeUnknownColor(unsigned char ch)
{
#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
    //unsigned char red = 1, green = 2;
#else
    //unsigned char red = 4, green = 2;
#endif
    return ch | 0x2400;//ComposeEditorChar('\0', red,green);
}

static inline EditorCharType MakeJSFerrorColor(unsigned char ch)
{
#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
    //unsigned char brgreen = 10, red = 1;
#else
    //unsigned char brgreen = 10, red = 4;
#endif
    return ch | 0x4A00;//ComposeEditorChar('\0', brgreen,red);
}

static inline EditorCharType MakeMenuColor(unsigned char ch)
{
    return ch | 0x7000;//ComposeEditorChar('\0', 0,7);
}

static inline EditorCharType MakeMarioColor(unsigned char ch)
{
    return ch | 0x0800;//ComposeEditorChar('\0', 8,0);
}

static /*inline*/ EditorCharType InvertColor(EditorCharType ch)
{
    if(sizeof(EditorCharType) > 2 && (ch & 0x80008000ul) == 0x80008000ul)
    {
        return ComposeEditorChar(ch, (unsigned char)(ch >> 16u),
                                     (unsigned char)( (unsigned(ch >> 8u)  & 0x7F)
                                                    | (unsigned(ch >> 23u) & 0x80) ),
                                     (ch >> 24u) & ~0x40);
    }
    else
    {
        //return ComposeEditorChar(ch, (ch>>12)&0xF, (ch>>8)&0xF, ch>>24);
        return (ch & ~EditorCharType(0xFF00u)) | ((ch << 4) & 0xF000u) | ((ch >> 4) & 0x0F00);
    }
}

static inline EditorCharType RecolorBgOnly(EditorCharType ch, EditorCharType attr)
{
    if(sizeof(EditorCharType) > 2)
        return (ch & ~0x00FF0000ul) | (attr & 0x00FF0000ul) | 0x80008000ul;
    return (ch & ~0xF000u) | (attr & 0xF000u);
}

static inline void VidmemPutEditorChar(EditorCharType ch, unsigned short*& Tgt)
{
    if(sizeof(EditorCharType) > 2)
    {
        Tgt[(DOSBOX_HICOLOR_OFFSET/2)] = (ch >> 16);
        if(FatMode) Tgt[(DOSBOX_HICOLOR_OFFSET/2)+1] = (ch >> 16);
    }
    *Tgt++ = ch;
    if(FatMode) *Tgt++ = ch | 0x80;
}
static inline EditorCharType VidmemReadEditorChar(unsigned short* Tgt)
{
    unsigned short attrlo = *Tgt;
    if(sizeof(EditorCharType) == 2) return attrlo;
    unsigned short attrhi = Tgt[(DOSBOX_HICOLOR_OFFSET/2)];
    unsigned long attr = attrlo | (((unsigned long)attrhi) << 16u);
    return attr;
}
