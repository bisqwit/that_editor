/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include "langdefs.hh"
#ifdef __BORLANDC__
# include <dos.h>
#endif

#ifdef __BORLANDC__
extern unsigned short* VidMem;
#define Pokeb(seg,ofs,v) *(unsigned char*)MK_FP(seg,ofs) = (v)
#define Pokew(seg,ofs,v) *(unsigned short*)MK_FP(seg,ofs) = (v)
#define Pokel(seg,ofs,v) *(unsigned long*)MK_FP(seg,ofs) = (v)
#define Peekb(seg,ofs) *(const unsigned char*)MK_FP(seg,ofs)
#define Peekw(seg,ofs) *(const unsigned short*)MK_FP(seg,ofs)
#define Peekl(seg,ofs) *(const unsigned long*)MK_FP(seg,ofs)

#elif defined(__DJGPP__)

#include <go32.h>
#include <sys/farptr.h>
#include <sys/nearptr.h>
// In order to access BIOS timer and VGA memory:
#define VidMem (reinterpret_cast<unsigned short*>(__djgpp_conventional_base + 0xB8000))
#define Pokeb(seg,ofs,v) _farpokeb(_dos_ds, (seg)*0x10+(ofs), (v))
#define Pokew(seg,ofs,v) _farpokew(_dos_ds, (seg)*0x10+(ofs), (v))
#define Pokel(seg,ofs,v) _farpokel(_dos_ds, (seg)*0x10+(ofs), (v))
#define Peekb(seg,ofs) _farpeekb(_dos_ds, (seg)*0x10+(ofs))
#define Peekw(seg,ofs) _farpeekw(_dos_ds, (seg)*0x10+(ofs))
#define Peekl(seg,ofs) _farpeekl(_dos_ds, (seg)*0x10+(ofs))
#define outport(r,b) outportw(r,b)

#else
static unsigned short VidMem[256*256];
#endif

#define DOSBOX_HICOLOR_OFFSET (-0x8000l)

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
