/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include "langdefs.hh"


#ifdef __BORLANDC__

extern unsigned short* VidMem;
#define DOSBOX_HICOLOR_OFFSET (-0x8000l)

#elif defined(__DJGPP__)

#include <sys/nearptr.h>
#define VidMem (reinterpret_cast<unsigned short*>(__djgpp_conventional_base + 0xB8000))
#define DOSBOX_HICOLOR_OFFSET (-0x8000l)

#else
#define DOSBOX_HICOLOR_OFFSET (-0x10000l)

extern unsigned short VideoBuffer[];
#define VidMem (VideoBuffer - DOSBOX_HICOLOR_OFFSET/2)
#endif

extern unsigned char VidW, VidH, VidCellHeight;
extern double VidFPS;
extern const unsigned char* VgaFont;
extern bool C64palette, FatMode, DispUcase, DCPUpalette;
extern int columns;

void VgaGetFont();
void VgaEnableFontAccess();
void VgaDisableFontAccess();
void VgaSetFont(unsigned char height, unsigned number, unsigned first, const unsigned char* source);
void VgaGetMode();
void VgaSetMode(unsigned modeno);
void VgaPutCursorAt(unsigned cx, unsigned cy, unsigned shape);

void VgaSetCustomMode(
    unsigned width,
    unsigned height,
    unsigned font_height,
    bool is_9pix,
    bool is_half/*horizontally doubled*/,
    bool is_double/*vertically doubled*/,
    int num_columns);

static inline unsigned short* GetVidMem(unsigned x, unsigned y, bool real=false)
{
    if(C64palette)
    {
        // Compensate for margins
        register unsigned offs = (x + 2) + (y + 2) * (VidW + 4);
        if(FatMode) offs <<= 1;
        return VidMem + offs;
    }
    else
    {
        register unsigned width = VidW * columns;
        if(real || columns == 1 || y == 0)
        {
            register unsigned offs = x + y * width;
            if(FatMode) offs <<= 1;
            return VidMem + offs;
        }
        register unsigned lines_per_real_screen = (VidH-1) / columns;
        register unsigned offs =
            x
          + ((y-1) % lines_per_real_screen + 1) * width
          + ((y-1) / lines_per_real_screen) * VidW;
        if(FatMode) offs <<= 1;
        return VidMem + offs;
    }
}
