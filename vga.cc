#include "vga.hh"

#include <stdlib.h> // malloc,free
#include <string.h> // memcpy
#include <stdio.h>

#ifdef __BORLANDC__
# include <dos.h>
unsigned short* VidMem = ((unsigned short *) MK_FP(0xB000, 0x8000));
#elif defined(__DJGPP__)
# include <dos.h>
# include <dpmi.h>
#else

unsigned short VideoBuffer[DOSBOX_HICOLOR_OFFSET*-2]; // Two pages
unsigned char  FontBuffer[256 * 32];
#include <SDL.h>
#include <vector>
#endif

#ifdef __BORLANDC__
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

#endif

//#ifdef __DJGPP__
//_go32_dpmi_seginfo font_memory_buffer{};
//#endif

static const unsigned char c64font[8*(256-64)] = {
#include "c64font.inc"
};
static const unsigned char p32font[32*256] = {
#include "8x32.inc"
};
static const unsigned char p19font[19*256] = {
#include "8x19.inc"
};
static const unsigned char p12font[12*256] = {
#include "8x12.inc"
};
static const unsigned char p10font[10*256] = {
#include "8x10.inc"
};
static const unsigned char p15font[15*256] = {
#include "8x15.inc"
};
static const unsigned char p32wfont[32*256] = {
#include "16x32.inc"
};
static const unsigned char dcpu16font[8*256] = {
#include "4x8.inc"
};
#if !defined(__BORLANDC__) && !defined(__DJGPP__)
// DOS versions do not need these font files,
// because they are supplied by the VGA BIOS.
static const unsigned char p16font[16*256] = {
#include "8x16.inc"
};
static const unsigned char p14font[14*256] = {
#include "8x14.inc"
};
static const unsigned char p8font[8*256] = {
#include "8x8.inc"
};
#endif

#if !(defined( __BORLANDC__) || defined(__DJGPP__))
extern volatile unsigned long MarioTimer;
namespace
{
    #define Make16(r,g,b) (((unsigned(b))&0x1F) | (((unsigned(g)<<1)&0x3F)<<5) | (((unsigned(r))&0x1F)<<11))
    static unsigned short xterm256table[256] =
        { Make16(0,0,0), Make16(21,0,0), Make16(0,21,0), Make16(21,5,0),
          Make16(0,0,21), Make16(21,0,21), Make16(0,21,21), Make16(21,21,21),
          Make16(7,7,7), Make16(31,5,5), Make16(5,31,5), Make16(31,31,5),
          Make16(5,5,31), Make16(31,5,31), Make16(5,31,31), Make16(31,31,31) };
    #include <cmath>
    static unsigned short xterm256_blend12[256][256];
    static struct xterm256init { xterm256init() {
        static const unsigned char grayramp[24] = { 1,2,3,5,6,7,8,9,11,12,13,14,16,17,18,19,20,22,23,24,25,27,28,29 };
        static const unsigned char colorramp[6] = { 0,12,16,21,26,31 };
        for(unsigned n=0; n<216; ++n) { xterm256table[16+n] = Make16(colorramp[(n/36)%6], colorramp[(n/6)%6], colorramp[(n)%6]); }
        for(unsigned n=0; n<24; ++n)  { xterm256table[232 + n] = Make16(grayramp[n],grayramp[n],grayramp[n]); }

        static constexpr double gamma = 2.0, ratio = 1./4;
        #define blend12(a, b, max) unsigned(\
           std::pow((std::pow(double(a/double(max)), gamma) * (1-ratio) \
                   + std::pow(double(b/double(max)), gamma) * ratio), 1/gamma) * max)

        #define blend12_16bit(c1, c2) \
            (blend12(((c1>> 0u)&0x1F), ((c2>> 0u)&0x1F), 0x1F) << 0u) \
          + (blend12(((c1>> 5u)&0x3F), ((c2>> 5u)&0x3F), 0x3F) << 5u) \
          + (blend12(((c1>>11u)&0x1F), ((c2>>11u)&0x1F), 0x1F) <<11u)
        for(unsigned a=0; a<256; ++a)
        for(unsigned b=0; b<256; ++b)
            xterm256_blend12[a][b] = blend12_16bit(xterm256table[a], xterm256table[b]);
        #undef blend12
    } } xterm256init;
    #undef Make16

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture*  texture  = nullptr;
    unsigned cells_horiz, cell_width_pixels,  pixels_width;
    unsigned cells_vert,  cell_height_pixels, pixels_height;
    unsigned cursor_x=0, cursor_y=0, cursor_shape=0;
    bool nine_pix = false, dbl_pix = false;
    std::vector<Uint16> pixbuf;
    void SDL_ReInitialize(unsigned cells_horizontal, unsigned cells_vertical,
                          bool cells_9pixwide,
                          bool cells_doublewide,
                          bool cells_doubletall)
    {
        nine_pix = cells_9pixwide;
        dbl_pix  = cells_doublewide;
        cells_horiz = cells_horizontal;
        cells_vert  = cells_vertical;
        cell_width_pixels  = (cells_9pixwide ? 9 : 8) * (cells_doublewide ? 2 : 1);
        cell_height_pixels = VidCellHeight * (cells_doubletall ? 2 : 1);
        pixels_width  = cells_horizontal * cell_width_pixels,
        pixels_height = cells_vertical   * cell_height_pixels;
        fprintf(stderr, "Cells: %ux%u, pix sizes: %ux%u (%u), pixels: %ux%u\n",
            cells_horiz,cells_vert,
            cell_width_pixels,cell_height_pixels, VidCellHeight,
            pixels_width,pixels_height);

        if(texture)
        {
            SDL_DestroyTexture(texture);
        }
        if(!window)
        {
            window = SDL_CreateWindow("editor",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                pixels_width, pixels_height,
                SDL_WINDOW_RESIZABLE);
        }
        else
        {
            SDL_SetWindowSize(window, pixels_width, pixels_height);
        }
        if(!renderer)
        {
            renderer = SDL_CreateRenderer(window, -1, 0);
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING, pixels_width, pixels_height);

        pixbuf.resize(pixels_width*pixels_height);
    }
    void SDL_ReDraw()
    {
        const unsigned vratio = (cell_height_pixels/VidCellHeight);
        #pragma omp parallel for
        for(unsigned py=0; py<pixels_height; ++py)
        {
            unsigned cy = py / cell_height_pixels;
            unsigned line = (py % cell_height_pixels) / vratio;
            const unsigned short* vidmem = VidMem + cy*cells_horiz;

            unsigned cursor_position = ~0u;
            if(cy == cursor_y
            && (MarioTimer & 8)
            && line >= (cursor_shape >> 8)
            && line <= (cursor_shape&0xFF))
            {
                cursor_position = cursor_x;
            }
            unsigned short* draw = &pixbuf[py*pixels_width];
            for(unsigned cx=0; cx<cells_horiz; ++cx)
            {
                unsigned short cell1 = vidmem[cx], cell2 = vidmem[cx + DOSBOX_HICOLOR_OFFSET/2];
                unsigned char chr  = cell1 & 0xFF, col = cell1 >> 8;
                unsigned char ext1 = cell2 & 0xFF, ext2 = cell2 >> 8;

                /*chr='A'; col=0x17; ext1=ext2=0;*/

                unsigned char font = FontBuffer[chr*32 + line];
                //unsigned char fg = col&0xF, bg = col>>4;
                if(ext2 == 7 && ext1 == 0x20) ext2 = ext1 = 0;
                bool flag_underline = ext2 & 0x01;
                bool flag_dim       = ext2 & 0x02;
                bool flag_italic    = ext2 & 0x04;
                bool flag_bold      = ext2 & 0x08;
                bool flag_blink     = ext2 & 0x20;
                bool flag_ext       = (ext2 & col & 0x80);
                if(!flag_ext)
                {
                    flag_blink = col >> 7;
                    ext1       = col>>4;   ext1 = (ext1&1)*4 + (ext1&2) + (ext1&4)/4 + (ext1&8);
                    col        = (col&15); col = (col&1)*4 + (col&2) + (col&4)/4 + (col&8);
                }
                //if(flag_blink && (MarioTimer & 32)) font = 0;

                unsigned widefont;
                if(cx == cursor_position)
                    widefont = 0xFF << 1;
                else
                {
                    widefont = font;

                    widefont <<= 1;
                    if(flag_italic && line < VidCellHeight*3/4) widefont >>= 1;

                    if(nine_pix && chr>=0xC0 && chr <= 0xDF)
                        widefont |= (widefont & 2) >> 1;

                    if((cursor_shape&0xFF) == 7)
                    {
                        // Check underline from LINE ABOVE
                        if(cy > 0)
                        {
                            unsigned short prev_ext = vidmem[cx + DOSBOX_HICOLOR_OFFSET/2 - cells_horiz];
                            unsigned char prev_ext1 = prev_ext & 0xFF, prev_ext2 = prev_ext >> 8;
                            if((prev_ext2 & 1) && line == 0) { /*widefont = 0x1FF;*/ /*bg =*/ ext1 = 8; }
                        }
                    }
                    else
                    {
                        if(flag_underline && line == (cursor_shape&0xFF))
                        {
                            //widefont = 0x1FF;
                            /*bg =*/ ext1 = 8;
                        }
                    }
                }
                /*if(flag_ext)*/
                {
                    unsigned fg_attr = (col & 0x7F) | ((ext2 & 0x40) << 1);
                    unsigned bg_attr = ext1;
                    Uint16 fg = xterm256table[fg_attr];
                    Uint16 bg = xterm256table[bg_attr];
                    /*if(flag_bold || flag_dim)
                    {
                        if(line&1) fg = fg + (0x4<<5) + (0x4<<0);
                        else       fg = fg + (0x4<<5) + (0x4<<11);
                    }*/

                    // 
                    const unsigned mode = flag_dim + flag_bold*2 + flag_italic*4*((line*4/VidCellHeight)%3);
                    static const unsigned char taketables[12][16] =
                    {
/*mode 0*/{0,0,0,0,3,3,3,3,0,0,0,0,3,3,3,3,},
/*mode 1*/{0,0,0,0,1,1,3,3,0,0,0,0,1,1,3,3,},
/*mode 2*/{0,0,0,0,3,3,3,3,1,1,1,1,3,3,3,3,},
/*mode 3*/{0,0,0,0,1,1,3,3,1,1,1,1,1,1,3,3,},
/*mode 4*/{0,0,1,1,2,2,3,3,0,0,1,1,2,2,3,3,},
/*mode 5*/{0,0,0,1,1,1,2,3,0,0,0,1,1,1,2,3,},
/*mode 6*/{0,0,1,1,2,2,3,3,1,1,2,2,2,2,3,3,},
/*mode 7*/{0,0,0,1,1,1,2,3,1,1,1,2,1,1,2,3,},
/*mode 8*/{0,0,2,2,1,1,3,3,0,0,2,2,1,1,3,3,},
/*mode 9*/{0,0,1,2,0,0,2,3,0,0,1,2,0,0,2,3,},
/*mode 10*/{0,0,2,2,2,2,3,3,0,0,2,2,2,2,3,3,},
/*mode 11*/{0,0,1,2,1,1,2,3,0,0,1,2,1,1,2,3,},
                    };
                    Uint16 colors[4] = {bg, xterm256_blend12[bg_attr][fg_attr], xterm256_blend12[fg_attr][bg_attr], fg};
                    if(dbl_pix)
                        for(unsigned i=0, j=nine_pix?9:8; i<j; ++i)
                        {
                            unsigned mask = ((widefont << 2) >> (8-i)) & 0xF;
                            unsigned c = colors[taketables[mode][mask]];
                            *draw++ = c;
                            *draw++ = c;
                        }
                    else
                        for(unsigned i=0, j=nine_pix?9:8; i<j; ++i)
                        {
                            unsigned mask = ((widefont << 2) >> (8-i)) & 0xF;
                            unsigned c = colors[taketables[mode][mask]];
                            *draw++ = c;
                        }
                }
            }
        }
        SDL_Rect rect; rect.w=pixels_width; rect.h=pixels_height; rect.y=0; rect.x=0;
        SDL_UpdateTexture(texture, &rect, pixbuf.data(), pixels_width*sizeof(pixbuf[0]));
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}
#endif


const unsigned char* VgaFont = 0;

unsigned char VidW=80, VidH=25, VidCellHeight=16;
double VidFPS = 60.0;
bool C64palette = false, FatMode = false, DispUcase = false, DCPUpalette = false;
int columns = 1;

void VgaGetFont()
{
  #if defined(__BORLANDC__) || defined(__DJGPP__)
    unsigned char mode = 1;
  #endif
    switch(VidCellHeight)
    {
        case 8:
            if(C64palette) { VgaFont = c64font; VgaFont -= 8*32; return; }
            if(DCPUpalette) { VgaFont = dcpu16font; return; }
          #if !defined(__BORLANDC__) && !defined(__DJGPP__)
            VgaFont = p8font; return;
          #else
            mode = 3; // ROM 8x8 double dot font
            break;
          #endif
        case 14:
          #if !defined(__BORLANDC__) && !defined(__DJGPP__)
            VgaFont = p14font; return;
          #else
            mode = 2; // ROM 8x14 font
            break;
          #endif
        case 16:
          #if !defined(__BORLANDC__) && !defined(__DJGPP__)
            VgaFont = p16font; return;
          #else
            mode = 6; // ROM 8x16 font
            break;
          #endif
        case 19: case 20: { VgaFont = p19font; return; }
        case 12: { VgaFont = p12font; return; }
        case 10: { VgaFont = p10font; return; }
        case 15: { VgaFont = p15font; return; }
        case 32: { VgaFont = FatMode ? p32wfont : p32font; return; }
        default:
          #if !defined(__BORLANDC__) && !defined(__DJGPP__)
            VgaFont = p8font; return;
          #else
            mode = 1; // INT 43h pointer (current 8x8 font)
            break;
          #endif
    }
#ifdef __BORLANDC__
    _asm {
        push es; push bp
        mov ax, 0x1130
        mov bh, mode
        int 0x10
        mov word ptr VgaFont+0, bp
        mov word ptr VgaFont+2, es
        xchg cx,cx
        xchg dx,dx
        pop bp
        pop es
    }
#elif defined(__DJGPP__)
    __dpmi_regs r{}; r.x.ax = 0x1130; r.h.bh = mode; __dpmi_int(0x10, &r);
    VgaFont = reinterpret_cast<const unsigned char*>(__djgpp_conventional_base + r.x.bp + r.x.es*0x10);
#else
    fprintf(stderr, "Unreachable\n");
#endif
}

void VgaEnableFontAccess()
{
#ifdef __DJGPP__
    // Get font access
    outport(0x3C4, 0x0100);
    outport(0x3C4, 0x0402);
    outport(0x3C4, 0x0704);
    outport(0x3C4, 0x0300);
    outport(0x3CE, 0x0204);
    outport(0x3CE, 0x0005);
    outport(0x3CE, 0x0406);
#endif
}
void VgaDisableFontAccess()
{
#ifdef __DJGPP__
    // Relinquish font access
    outport(0x3C4, 0x0100);
    outport(0x3C4, 0x0302);
    outport(0x3C4, 0x0304);
    outport(0x3C4, 0x0300);
    outport(0x3CE, ((inportb(0x3CC) & 1) << 10) | 0xA06);
    outport(0x3CE, 0x0004);
    outport(0x3CE, 0x1005);
#endif
}
void VgaSetFont(unsigned char height, unsigned number, unsigned first, const unsigned char* source)
{
#ifdef __BORLANDC__
    _asm {
        push es
        push bp
         les bp, source
         mov ax, 0x1100
         mov bh, height
         mov bl, 0
         mov cx, number
         mov dx, first
         int 0x10
        pop bp
        pop es
    }
#else
  #if 0
    auto& g = font_memory_buffer;
    if(!g.size)
    {
        g.size = 256 * 32; // maximum estimated size
        _go32_dpmi_allocate_dos_memory(&g);
    }
    unsigned size_bytes = height * number;
    memcpy(reinterpret_cast<char*>(__djgpp_conventional_base + g.rm_segment*0x10+g.rm_offset),
           source,
           size_bytes);
    //REGS r{}; r.w.ax = 0x1100; r.h.bh = height; r.w.cx = number; r.w.dx = first;
    //SREGS s{g.rm_segment,g.rm_segment,g.rm_segment, g.rm_segment,g.rm_segment,g.rm_segment}; r.w.bp = g.rm_offset;
    //int86x(0x10, &r,&r, &s);
    __dpmi_regs r{}; r.x.ax = 0x1100; r.h.bh = height; r.x.cx = number; r.x.dx = first;
    r.x.ds = r.x.es = g.rm_segment; r.x.bp = g.rm_offset;
    __dpmi_int(0x10, &r);
  #elif defined(__DJGPP__)
    unsigned tgt = __djgpp_conventional_base + 0xA0000;
    for(unsigned c=0; c<number; ++c)
        __builtin_memcpy(reinterpret_cast<char*>(tgt + (first+c)*32), source + c*height, height);
  #else
    //fprintf(stderr, "Font init (%u-%u, height=%u)\n", first,first+number, height);
    for(unsigned c=0; c<number; ++c)
        memcpy(FontBuffer + (first+c)*32, source + c*height, height);
  #endif
#endif
}

void VgaGetMode()
{
#ifdef __BORLANDC__
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl; mov VidCellHeight, cl }
    _asm { mov ax, 0x1003; xor bx,bx; int 0x10 } // Disable blink-bit

#elif defined(__DJGPP__)

    {REGS r{}; r.h.ah = 0xF; int86(0x10,&r,&r); VidW = r.h.ah;
    r.w.ax = 0x1130; r.w.bx = 0; int86(0x10,&r,&r); VidH = r.h.dl; VidCellHeight = r.h.cl;
    r.w.ax = 0x1003; r.w.bx = 0; int86(0x10,&r,&r);} // Disable blink-bit

#else
    VidW = cells_horiz;
    VidH = cells_vert;
    VidCellHeight = cell_height_pixels;
#endif

    if(VidH == 0) VidH = 25; else VidH += 1;
    VgaGetFont();

    if(FatMode) VidW /= 2;
    if(C64palette) { VidW -= 4; VidH -= 5; }
    if(columns)
    {
        VidW /= columns;
        VidH = (VidH-1) * columns + 1;
    }
}

#if defined(__BORLANDC__) || defined(__DJGPP__)
void VgaSetMode(unsigned modeno)
{
  #ifdef __BORLANDC__
    if(modeno < 0x100)
        _asm { mov ax, modeno; int 0x10 }
    else
        _asm { mov bx, modeno; mov ax, 0x4F02; int 0x10 }
  #endif
  #ifdef __DJGPP__
    if(modeno < 0x100)
        { REGS r{}; r.w.ax = modeno; int86(0x10,&r,&r); }
    else
        { REGS r{}; r.w.bx = modeno; r.w.ax = 0x4F02; int86(0x10,&r,&r); }
  #endif

    static const unsigned long c64pal[16] =
    {
        0x3E31A2ul, // 0
        0x7C70DAul, //0x000000ul,
        0x68A141ul,
        0x7ABFC7ul,
        0xFCFCFCul, // 4
        0x8A46AEul,
        0x905F25ul,
        0x7C70DAul,
        0x3E31A2ul, // 8
        0x7C70DAul,
        0xACEA88ul, 
        0x7ABFC7ul,
        0xBB776Dul, // C
        0x8A46AEul,
        0xD0DC71ul,
        0x7ABFC7ul
    };
    static const unsigned long dcpu16pal[16] =
    {
        0x000000ul,0x005784ul,0x44891Aul,0x2F484Eul,
        0xBE2633ul,0x493C2Bul,0xA46422ul,0x9D9D9Dul,
        0x1B2632ul,0x31A2F2ul,0xA3CE27ul,0xB2DCEFul,
        0xE06F8Bul,0xEB8931ul,0xFFE26Bul,0xFFFFFFul
    };

    static const unsigned long replacementpal[16] =
    {
        0x000000ul,0x0000AAul,0x00AA00ul,0x00AAAAul,
        0xAA0000ul,0xAA00AAul,0xAA5500ul,0xAAAAAAul,
        0x555555ul,0x5555FFul,0x55FF55ul,0x55FFFFul,
        0xFF5555ul,0xFF55FFul,0xFF5555ul,0xFFFFFFul
    };

    const unsigned long* extra_pal = replacementpal;
    if(C64palette) extra_pal = c64pal;
    if(DCPUpalette) extra_pal = dcpu16pal;
    #if 0
    static const unsigned long primerpal[16] =
    {
        0x000000ul,0x00005Ful,0x68A141ul,0x7ABFC7ul,
        0xD75F5Ful,0x493C2Bul,0xA46422ul,0xD7D7D7ul,
        0x878787ul,0x31A2F2ul,0xA3CE27ul,0xB2DCEFul,
        0xFFAF5Ful,0xAF5FFFul,0xFFFFAFul,0xFFFFFFul
    };
    //extra_pal = primerpal; // Special Primer version
    #endif
    #if 0
    static const unsigned long replacementpal_test[16] =
    {
        0x000000ul,0x0000AAul,0x00AA00ul,0x00AAAAul,
        0xAA0000ul,0xAA00AAul,0x88A8C0ul,0xAAAAAAul,
        0x555555ul,0x5555FFul,0x55FF55ul,0x55FFFFul,
        0xFF5555ul,0xFF55FFul,0xFFFF55ul,0xFFFFFFul
    };
    //extra_pal = replacementpal_test;
    #endif

    outportb(0x3C8, 0x20);
    for(unsigned a=0; a<16; ++a)
    {
        outportb(0x3C9, extra_pal[a] >> 18);
        outportb(0x3C9, (extra_pal[a] >> 10) & 0x3F);
        outportb(0x3C9, (extra_pal[a] >> 2) & 0x3F);
    }
}
#endif

void VgaSetCustomMode(
    unsigned width,
    unsigned height,
    unsigned font_height,
    bool is_9pix,
    bool is_half/*horizontally doubled*/,
    bool is_double/*vertically doubled*/,
    int num_columns)
{
    columns = num_columns;

    if(C64palette)
    {
        width  += 4;
        height += 5;
    }
    if(FatMode)
        width *= 2;

    VidW = width;
    VidH = height;

    // When columns are used:
    if(num_columns)
    {
        width *= num_columns;
        height = (height-1) / num_columns + 1;
    }

  #if defined( __BORLANDC__) || defined(__DJGPP__)
    unsigned hdispend = width;
    unsigned vdispend = height*font_height;
    if(is_double) vdispend *= 2;
    unsigned htotal = width*5/4;
    unsigned vtotal = vdispend+45;
    Pokeb(0x40, 0x85, font_height);
  #endif

    {// Set an empty 32-pix font
    static const unsigned char emptyfont[32] = {0};
    VgaEnableFontAccess();
    for(unsigned n=0; n<256; ++n) VgaSetFont(32, 1,n, emptyfont);
    VgaDisableFontAccess();}

    VgaEnableFontAccess();
    switch(font_height)
    {
        case 32: VgaSetFont(32,256,0, FatMode ? p32wfont : p32font); break;
        case 19: VgaSetFont(19,256,0, p19font); break;
        case 12: VgaSetFont(12,256,0, p12font); break;
        case 10: VgaSetFont(10,256,0, p10font); break;
        case 15: VgaSetFont(15,256,0, p15font); break;
    #if !(defined( __BORLANDC__) || defined(__DJGPP__))
        case 16: VgaSetFont(16,256,0, p16font); break;
        case 14: VgaSetFont(14,256,0, p14font); break;
        case  8: VgaSetFont( 8,256,0,  p8font); break;
        default: fprintf(stderr, "WARNING: NO FONT WAS SET\n");
    #endif
    }
    VgaDisableFontAccess();

    if(font_height == 16)
    {
        // Set standard 80x25 mode as a baseline
        // This triggers font reset on JAINPUT.
      #if defined( __BORLANDC__) || defined(__DJGPP__)
        Pokeb(0x40,0x87, Peekb(0x40,0x87)|0x80); // tell BIOS to not clear VRAM
        VgaSetMode(3);
        Pokeb(0x40,0x87, Peekb(0x40,0x87)&~0x80);
      #endif
    }

    if(font_height ==14) {
        #ifdef __BORLANDC__
        _asm { mov ax, 0x1101; mov bl, 0; int 0x10 }
        #elif defined(__DJGPP__)
        REGS r{}; r.w.ax = 0x1101; r.h.bl = 0; int86(0x10,&r,&r);
        #endif
    }
    if(font_height == 8) {
        #ifdef __BORLANDC__
        _asm { mov ax, 0x1102; mov bl, 0; int 0x10 }
        #elif defined(__DJGPP__)
        REGS r{}; r.w.ax = 0x1102; r.h.bl = 0; int86(0x10,&r,&r);
        #endif
        VgaEnableFontAccess();
        if(DCPUpalette) VgaSetFont(8,256, 0, dcpu16font);
        if(C64palette) VgaSetFont(8, 256-64, 32,c64font); // Contains 20..DF
    }


  #if defined( __BORLANDC__) || defined(__DJGPP__)
    /* This script is, for the most part, copied from DOSBox. */
    {unsigned long seq = 0x60300UL; if(!is_9pix) seq|=0x10; if(is_half) seq|=0x80;
    for(unsigned a=0; a<5; ++a) outport(0x3C4, a | (((seq >> (a*4))&0xF) << 8));}

    // Disable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)&0x7F);
    // crtc
    {for(unsigned a=0; a<=0x18; ++a) outport(0x3D4, a | 0x0000);}
    // Rewritten indexes: 0 1 2 3 4 5 6 7 9 10 11 12 13 14 15 16 17 18 51 5D 5E*2 69
    // Not rewritten:     8 A..F
    unsigned overflow = 0, max_scanline = 0, ver_overflow = 0, hor_overflow = 0;
    outport(0x3D4, 0x00 | ((htotal-5) << 8));   hor_overflow |= ((htotal-5) >> 8) << 0;
    outport(0x3D4, 0x01 | ((hdispend-1) << 8)); hor_overflow |= ((hdispend-1) >> 8) << 1;
    outport(0x3D4, 0x02 | ((hdispend) << 8));   hor_overflow |= ((hdispend) >> 8) << 2;
    {register unsigned blank_end = (htotal-2) & 0x7F;
    outport(0x3D4, 0x03 | ((0x80|blank_end) << 8));
    {register unsigned ret_start = is_half ? (hdispend+3) : (hdispend+5);
    outport(0x3D4, 0x04 | (ret_start << 8));    hor_overflow |= ((ret_start) >> 8) << 4;}
    {register unsigned ret_end = is_half ? ((htotal-18)&0x1F)|(is_double?0x20:0) : ((htotal-4)&0x1F);
    outport(0x3D4, 0x05 | ((ret_end | ((blank_end & 0x20) << 2)) << 8));}}
    outport(0x3D4, 0x06 | ((vtotal-2) << 8));
    overflow |= ((vtotal-2) & 0x100) >> 8;
    overflow |= ((vtotal-2) & 0x200) >> 4;
    ver_overflow |= (((vtotal-2) & 0x400) >> 10);
    {register unsigned vretrace = vdispend + (4800/vdispend + (vdispend==350?24:0));
    outport(0x3D4, 0x10 | (vretrace << 8));
    overflow |= (vretrace & 0x100) >> 6;
    overflow |= (vretrace & 0x200) >> 2;
    ver_overflow |= (vretrace & 0x400) >> 6;
    outport(0x3D4, 0x11 | (((vretrace+2) & 0xF) << 8));}
    outport(0x3D4, 0x12 | ((vdispend-1) << 8));
    overflow |= ((vdispend-1) & 0x100) >> 7;
    overflow |= ((vdispend-1) & 0x200) >> 3;
    ver_overflow |= ((vdispend-1) & 0x400) >> 9;
    {register unsigned vblank_trim = vdispend / 66;
    outport(0x3D4, 0x15 | ((vdispend+vblank_trim) << 8));
    overflow |= ((vdispend+vblank_trim) & 0x100) >> 5;
    max_scanline |= ((vdispend+vblank_trim) & 0x200) >> 4;
    ver_overflow |= ((vdispend+vblank_trim) & 0x400) >> 8;
    outport(0x3D4, 0x16 | ((vtotal-vblank_trim-2) << 8));}
    {register unsigned line_compare = vtotal < 1024 ? 1023 : 2047;
    outport(0x3D4, 0x18 | (line_compare << 8));
    overflow |= (line_compare & 0x100) >> 4;
    max_scanline |= (line_compare & 0x200) >> 3;
    ver_overflow |= (line_compare & 0x400) >> 4;}
    if(is_double) max_scanline |= 0x80;
    max_scanline |= font_height-1;
    {register unsigned underline = 0x1F; //vdispend == 350 ? 0x0F : 0x1F;
    outport(0x3D4, 0x09 | (max_scanline << 8));
    outport(0x3D4, 0x14 | (underline << 8));}
    outport(0x3D4, 0x07 | (overflow << 8));
    outport(0x3D4, 0x5D | (hor_overflow << 8)); // S3 trio extension
    outport(0x3D4, 0x5E | (ver_overflow << 8)); // S3 trio extension
    {register unsigned offset = hdispend / 2;
    outport(0x3D4, 0x13 | (offset << 8));
    outport(0x3D4, 0x51 | (((offset & 0x300) >> 4) << 8));}
    outport(0x3D4, 0x69 | (0 << 8)); // S3 trio
    outport(0x3D4, 0x5E | (ver_overflow << 8)); // (repeat for some reason)
    {register unsigned modecontrol = 0xA3;
    outport(0x3D4, 0x17 | (modecontrol << 8));}

    // Enable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)|0x80);
    outport(0x3D4, 0x67 | (0 << 8)); // S3 trio

    unsigned misc_output = 0x63;/*0x02*/
    outportb(0x3C2, misc_output); // misc output

    {static const unsigned char Gfx[9] = { 0,0,0,0,0,0x10,0x0E,0x0F,0xFF };
    for(unsigned a=0; a<9; ++a) outport(0x3CE, a | (Gfx[a] << 8));}

    {static unsigned char Att[0x15] = { 0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
                                        0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
                                        0, 0x00,0x0F, 0, 0 };
     Att[0x10] = int(is_9pix)*4;
     Att[0x13] = int(is_9pix)*8;
    if(C64palette || (DCPUpalette && font_height == 8)) //DCPU/Primer/C64 hack
    {
        for(unsigned a=0; a<0x10; ++a) Att[a] = 0x20+a;
    }

    inportb(0x3DA);
    {for(unsigned a=0x0; a<sizeof(Att); ++a)
        { if(a == 0x11) continue;
          outportb(0x3C0, a);
          outportb(0x3C0, Att[a]); }} }

    inportb(0x3DA);
    outportb(0x3C0, 0x20);

    Pokeb(0x40, 0x10, (Peekb(0x40,0x10) & ~0x30) | 0x20);
    Pokeb(0x40, 0x65, 0x29);
    Pokeb(0x40, 0x4A, width);
    Pokeb(0x40, 0x84, height-1);
    Pokew(0x40, 0x4C, width*height*2);
  #endif

#if defined( __BORLANDC__) || defined(__DJGPP__)
    double clock = 28322000.0;
    if(((misc_output >> 2) & 3) == 0) clock = 25175000.0;
    clock /= (is_9pix ? 9.0 : 8.0);
    clock /= vtotal;
    clock /= htotal;
    if(is_half)   clock /= 2.0;
    VidFPS = clock;
#else
    VidCellHeight = font_height;
    SDL_ReInitialize(width, height, is_9pix, is_half, is_double);
#endif

    // At this point, width & height indicate the true number of 8-pix wide cells.
    // Now correct them for actual number of printable characters.
    if(FatMode)
    {
        width /= 2;
    }
    if(C64palette)
    {
        memset(VidMem, 0x99, width*height*(FatMode?4:2));
        width -= 4; height -= 5;
    }

    VidCellHeight = font_height;
    VgaGetFont();
}

void VgaPutCursorAt(unsigned cx, unsigned cy, unsigned shape)
{
#if defined(__BORLANDC__) || defined(__DJGPP__)
    if(columns > 1)
    {
        register unsigned short h = (VidH-1) / columns;
        cx += ((cy-1) / h) * VidW;
        cy =  ((cy-1) % h) + 1;
    }
    if(FatMode) cx *= 2;
    unsigned char cux = cx, cuy = cy;

    #ifdef __DJGPP__
    unsigned addr = cux + cuy*VidW;
    _farpokew(_dos_ds, 0x450, (cuy<<8) + cux);
    outportw(0x3D4, 0x0E + (addr&0xFF00));
    outportw(0x3D4, 0x0F + ((addr&0xFF)<<8));
    // Set cursor shape: lines-4 to lines-3, or 6 to 7
    outportw(0x3D4, 0x0A + ((shape & 0xFF00))   );
    outportw(0x3D4, 0x0B + ((shape & 0xFF) << 8));
    #else
    _asm { mov ah, 2; mov bh, 0; mov dh, cuy; mov dl, cux; int 0x10 }
    _asm { mov ah, 1; mov cx, shape; int 0x10 }
    *(unsigned char*)MK_FP(0x40,0x50) = cx;
    *(unsigned char*)MK_FP(0x40,0x51) = cy;
    #endif
#else
    cursor_x = cx;
    cursor_y = cy;
    cursor_shape = shape;
#endif
}

#if !(defined(__BORLANDC__) || defined(__DJGPP__))
static unsigned long last_timer;
void VgaRedraw()
{
    unsigned long tick = MarioTimer;
    if(tick != last_timer)
    {
        last_timer = tick;
        SDL_ReDraw();
    }
}
#endif
