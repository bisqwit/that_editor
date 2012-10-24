#ifndef bqtEattrHH
#define bqtEattrHH

#include <cstdint>
#include <string>

#include "types.hh"

/* Some constexpr functions for converting various formats of colors into RGB15 */

static constexpr unsigned short RGBtoRGB15(unsigned r,unsigned g,unsigned b)
{
    return (r*31/255) * 32*32 + (g*31/255) * 32 + (b*31/255);
}
static constexpr unsigned short RGBtoRGB15(unsigned rgb)
{
    return RGBtoRGB15( (rgb>>16)&0xFF, (rgb>>8)&0xFF, (rgb>>0)&0xFF );
}

static constexpr unsigned short EGAcolorTable[16] =
{
    RGBtoRGB15(0x00,0x00,0x00), RGBtoRGB15(0x00,0x00,0xAA),
    RGBtoRGB15(0x00,0xAA,0x00), RGBtoRGB15(0x00,0xAA,0xAA),
    RGBtoRGB15(0xAA,0x00,0x00), RGBtoRGB15(0xAA,0x00,0xAA),
    RGBtoRGB15(0xAA,0x55,0x00), RGBtoRGB15(0xC0,0xC0,0xC0),
    RGBtoRGB15(0x88,0x88,0x88), RGBtoRGB15(0x55,0x55,0xFF),
    RGBtoRGB15(0x55,0xFF,0x55), RGBtoRGB15(0x55,0xFF,0xFF),
    RGBtoRGB15(0xFF,0x55,0x55), RGBtoRGB15(0xFF,0x55,0xFF),
    RGBtoRGB15(0xFF,0xFF,0x55), RGBtoRGB15(0xFF,0xFF,0xFF)
};
static constexpr unsigned short EGAtoRGB15(char color)
{
    return EGAcolorTable[color & 0xF];
}


static constexpr unsigned char Xterm256Table[6] =
{
    0x00,0x5F,0x87,0xAF,0xD7,0xFF
};
static constexpr unsigned short XtermToRGB15(unsigned r,unsigned g,unsigned b)
{
    return RGBtoRGB15( Xterm256Table[r], Xterm256Table[g], Xterm256Table[b] );
}

struct AttrType
{
    union
    {
        uint_least32_t as_int;
        RegBitSet< 0,15, uint_least32_t> foreground;
        RegBitSet<15,15, uint_least32_t> background;
        RegBitSet<30, 2, uint_least32_t> underline_type;
        RegBitSet< 0, 5, uint_least32_t> fg_blue;
        RegBitSet< 5, 5, uint_least32_t> fg_green;
        RegBitSet<10, 5, uint_least32_t> fg_red;
        RegBitSet<15, 5, uint_least32_t> bg_blue;
        RegBitSet<20, 5, uint_least32_t> bg_green;
        RegBitSet<25, 5, uint_least32_t> bg_red;
    } data;

    constexpr AttrType(char fg, char bg)
        : AttrType( EGAtoRGB15(fg), EGAtoRGB15(bg) )
    {
    }

    constexpr AttrType(unsigned short fg, unsigned short bg, unsigned char und = 0)
        : data{ (unsigned)fg + (unsigned)( unsigned(bg) << 15 ) + (unsigned)( unsigned(und) << 30 ) }
    {
    }

    constexpr AttrType(unsigned fg, unsigned bg)
        : AttrType( RGBtoRGB15(fg), RGBtoRGB15(bg) )
    {
    }

    constexpr AttrType() : AttrType('\4', '\2')
    {
    }

    static AttrType ParseJSFattribute(char* line);

    bool operator==(const AttrType& b) const
    {
        return data.as_int == b.data.as_int;
    }
    bool operator!=(const AttrType& b) const
    {
        return !operator==(b);
    }
};


static constexpr AttrType UnknownColor   = AttrType{'\4',  '\2'};
static constexpr AttrType AttrParseError = AttrType{'\12', '\4'};
static constexpr AttrType BlankColor     = AttrType{'\7',  '\0'};

#endif
