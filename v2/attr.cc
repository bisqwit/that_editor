#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "attr.hh"

AttrType AttrType::ParseJSFattribute(char* line)
{
    auto length = std::strlen(line);

    // Is it a two-character hexacode? Interpet as EGA color
    if(length == 2)
    {
        char* endptr = nullptr;
        int ega_attr = std::strtol(line, &endptr, 16);
        if(!*endptr)
            return AttrType( char(ega_attr & 0xF),
                             char(ega_attr >> 4 ) );
    }
    // Three-character? Interpret as RGB, foreground only (background black)
    if(length == 3)
    {
        char* endptr = nullptr;
        int rgb_attr = std::strtol(line, &endptr, 16);
        if(!*endptr)
            return AttrType( RGBtoRGB15(255*((rgb_attr>>8)&0xF)/15,
                                        255*((rgb_attr>>4)&0xF)/15,
                                        255*((rgb_attr>>0)&0xF)/15
                                       ), (unsigned short)0u );
    }
    // Six-character? Interpret as RGB, 24-bt
    if(length == 6)
    {
        char* endptr = nullptr;
        int rgb_attr = std::strtol(line, &endptr, 16);
        if(!*endptr)
            return AttrType( RGBtoRGB15(((rgb_attr>>16)&0xFF),
                                        ((rgb_attr>>8)&0xFF),
                                        ((rgb_attr>>0)&0xFF)
                                       ), (unsigned short) 0u );
    }
    // Okay, it is more complex than that.
    // Assume it is a Joe full-featured syntax.
    // Tokenize it (split into words).
    bool dim=false, underline=false, blink=false;
    bool italic=false, inverse=false, bold=false;
    unsigned short fg=0;
    unsigned short bg=0;

    auto HandleWord = [&](const char* word)
    {
        #define w(s, action) else if(!std::strcmp(word, s)) { action; }
        if(0) {}
        w("dim",       dim       = true)
        w("underline", underline = true)
        w("blink",     blink     = true)
        w("italic",    italic    = true)
        w("inverse",   inverse   = true)
        w("bold",      bold      = true)
        w("black",     fg= 0) w("bg_black",     bg= 0)
        w("blue",      fg= 1) w("bg_blue",      bg= 1)
        w("green",     fg= 2) w("bg_green",     bg= 2)
        w("cyan",      fg= 3) w("bg_cyan",      bg= 3)
        w("red",       fg= 4) w("bg_red",       bg= 4)
        w("reg",       fg= 4) w("bg_reg",       bg= 4)
        w("magenta",   fg= 5) w("bg_magenta",   bg= 5)
        w("yellow",    fg= 6) w("bg_yellow",    bg= 6)
        w("white",     fg= 7) w("bg_white",     bg= 7)
        w("BLACK",     fg= 8) w("BG_BLACK",     bg= 8)
        w("BLUE",      fg= 9) w("BG_BLUE",      bg= 9)
        w("GREEN",     fg=10) w("BG_GREEN",     bg=10)
        w("CYAN",      fg=11) w("BG_CYAN",      bg=11)
        w("RED",       fg=12) w("BG_RED",       bg=12)
        w("REG",       fg=12) w("BG_REG",       bg=12)
        w("MAGENTA",   fg=13) w("BG_MAGENTA",   bg=13)
        w("YELLOW",    fg=14) w("BG_YELLOW",    bg=14)
        w("WHITE",     fg=15) w("BG_WHITE",     bg=15)
        else if(!std::strncmp(word, "bg_", 3) == 0
              && word[3]>='0' && word[3]<='5'
              && word[4]>='0' && word[4]<='5'
              && word[5]>='0' && word[5]<='5'
              && word[6]=='\0')
        {
            bg = 10000 + (word[5]-'0') + 16*(word[4]-'0') + 256*(word[3]-'0');
        }
        else if(!std::strncmp(word, "fg_", 3) == 0
              && word[3]>='0' && word[3]<='5'
              && word[4]>='0' && word[4]<='5'
              && word[5]>='0' && word[5]<='5'
              && word[6]=='\0')
        {
            fg = 10000 + (word[5]-'0') + 16*(word[4]-'0') + 256*(word[3]-'0');
        }
        else
        {
            std::fprintf(stdout, "Invalid color in JSF file: '%s'\n", word);
            std::fflush(stdout);
        }
    };
    for(char* p = line; *p; )
    {
        if(*p == ' ' || *p == '\t')
        {
            *p = '\0';
            if(*line) HandleWord(line);
            line = ++p;
        }
        else ++p;
    }
    if(*line) HandleWord(line);

    if(fg >= 10000)
        fg = XtermToRGB15( ((fg-10000) >> 8) & 7, ((fg-10000) >> 4) & 7, ((fg-10000) >> 0) & 7 );
    else
    {
        if(bold) fg |= 8;
        fg = EGAtoRGB15(fg);
    }

    if(bg >= 10000)
        bg = XtermToRGB15( ((bg-10000) >> 8) & 7, ((bg-10000) >> 4) & 7, ((bg-10000) >> 0) & 7 );
    else
    {
        if(blink) bg |= 8;
        bg = EGAtoRGB15(bg);
    }

    if(dim)
        fg = ((((fg >> 10) & 31) * 2 / 3) << 10)
           | ((((fg >>  5) & 31) * 2 / 3) <<  5)
           | ((((fg >>  0) & 31) * 2 / 3) <<  0);

    if(inverse)
        std::swap(fg, bg);

    unsigned char und = 0;
    if(underline) und |= 1;
    if(italic)    und |= 2;

    return AttrType(fg, bg, und);
}
