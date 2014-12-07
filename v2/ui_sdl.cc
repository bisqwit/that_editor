#include <cstdint>
#include <algorithm>

#include "ui.hh"

#include "fonts/16x32.inc"
#include "fonts/8x32.inc"
#include "fonts/8x16.inc"
#include "fonts/8x19.inc"
#include "fonts/c64font.inc"
#include "fonts/6x9.inc"

static const struct { int x,y; UIfontBase (* Get)(); } fonts[] =
{
    // 8x8, 9x8
    { 6,9,  Getfont6x9 },

    { 8,16, Getfont8x16 },
    { 8,19, Getfont8x19 },
    { 8,32, Getfont8x32 },
    {16,32, Getfont16x32 },
    { 8, 8, Getfontc64font },

    { 9,16, Getfont8x16 },
    { 9,19, Getfont8x19 },
    { 9,32, Getfont8x32 },
};


class UI_DOS_VGA: public UI
{
public:
    virtual void SetMode(
        /* Width and height in character cells */
        unsigned width,
        unsigned height,
        /* Font specification */
        unsigned font_width,
        unsigned font_height,
        bool     font_x_double,
        bool     font_y_double,
        /* Special transformations */
        bool     C64mode
    );
};

void UI_DOS_VGA::SetMode(
    /* Width and height in character cells */
    unsigned width,
    unsigned height,
    /* Font specification */
    unsigned font_width,
    unsigned font_height,
    bool     font_x_double,
    bool     font_y_double,
    /* Special transformations */
    bool     C64mode
)
{
    
}
