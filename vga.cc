#include "vga.hh"

#include <stdlib.h> // malloc,free
#include <string.h> // memcpy
#include <stdio.h>

#ifdef __BORLANDC__
# include <dos.h>
unsigned short* VidMem = ((unsigned short *) MK_FP(0xB000, 0x8000));
#endif
#ifdef __DJGPP__
# include <dos.h>
# include <dpmi.h>
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


const unsigned char* VgaFont = 0;

unsigned char VidW=80, VidH=25, VidCellHeight=16;
double VidFPS = 60.0;
bool C64palette = false, FatMode = false, DispUcase = false, DCPUpalette = false;
int columns = 1;


void VgaGetFont()
{
    unsigned char mode = 1;
    switch(VidCellHeight)
    {
        case 8:
            if(C64palette) { VgaFont = c64font-8*32; return; }
            if(DCPUpalette) { VgaFont = dcpu16font; return; }
            mode = 3; break;
        case 14: mode = 2; break;
        case 16: mode = 6; break;
        case 19: case 20: { VgaFont = p19font; return; }
        case 12: { VgaFont = p12font; return; }
        case 10: { VgaFont = p10font; return; }
        case 15: { VgaFont = p15font; return; }
        case 32: { VgaFont = FatMode ? p32wfont : p32font; return; }
        default: mode = 1; break;
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
#else
    __dpmi_regs r{}; r.x.ax = 0x1130; r.h.bh = mode; __dpmi_int(0x10, &r);
    VgaFont = reinterpret_cast<const unsigned char*>(__djgpp_conventional_base + r.x.bp + r.x.es*0x10);
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
  #else
    unsigned tgt = __djgpp_conventional_base + 0xA0000;
    for(unsigned c=0; c<number; ++c)
        __builtin_memcpy(reinterpret_cast<char*>(tgt + (first+c)*32), source + c*height, height);
  #endif
#endif
}

void VgaGetMode()
{
#ifdef __BORLANDC__
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl; mov VidCellHeight, cl }
    _asm { mov ax, 0x1003; xor bx,bx; int 0x10 } // Disable blink-bit
#endif
#ifdef __DJGPP__
    {REGS r{}; r.h.ah = 0xF; int86(0x10,&r,&r); VidW = r.h.ah;
    r.w.ax = 0x1130; r.w.bx = 0; int86(0x10,&r,&r); VidH = r.h.dl; VidCellHeight = r.h.cl;
    r.w.ax = 0x1003; r.w.bx = 0; int86(0x10,&r,&r);} // Disable blink-bit
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

void VgaSetMode(unsigned modeno)
{
#if defined(__BORLANDC__) || defined(__DJGPP__)
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
    #endif
    static const unsigned long primerpal[16] =
    {
        0x000000ul,0x00005Ful,0x68A141ul,0x7ABFC7ul,
        0xD75F5Ful,0x493C2Bul,0xA46422ul,0xD7D7D7ul,
        0x878787ul,0x31A2F2ul,0xA3CE27ul,0xB2DCEFul,
        0xFFAF5Ful,0xAF5FFFul,0xFFFFAFul,0xFFFFFFul
    };
    //extra_pal = primerpal; // Special Primer version
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
#endif
}

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

    {// Set an empty 32-pix font
    static const unsigned char emptyfont[32] = {0};
    VgaEnableFontAccess();
    for(unsigned n=0; n<256; ++n) VgaSetFont(32, 1,n, emptyfont);
    VgaDisableFontAccess();}

    if(font_height == 16)
    {
        // Set standard 80x25 mode as a baseline
        // This triggers font reset on JAINPUT.
        Pokeb(0x40,0x87, Peekb(0x40,0x87)|0x80); // tell BIOS to not clear VRAM
        VgaSetMode(3);
        Pokeb(0x40,0x87, Peekb(0x40,0x87)&~0x80);
    }

    if(font_height ==14) {
        #ifdef __BORLANDC__
        _asm { mov ax, 0x1101; mov bl, 0; int 0x10 }
        #else
        REGS r{}; r.w.ax = 0x1101; r.h.bl = 0; int86(0x10,&r,&r);
        #endif
    }
    if(font_height == 8) {
        #ifdef __BORLANDC__
        _asm { mov ax, 0x1102; mov bl, 0; int 0x10 }
        #else
        REGS r{}; r.w.ax = 0x1102; r.h.bl = 0; int86(0x10,&r,&r);
        #endif
        VgaEnableFontAccess();
        if(DCPUpalette) VgaSetFont(8,256, 0, dcpu16font);
        if(C64palette) VgaSetFont(8, 256-64, 32,c64font); // Contains 20..DF
    }

    VgaEnableFontAccess();
    if(font_height == 32) VgaSetFont(32,256,0, FatMode ? p32wfont : p32font);
    if(font_height == 19) VgaSetFont(19,256,0, p19font);
    if(font_height == 12) VgaSetFont(12,256,0, p12font);
    if(font_height == 10) VgaSetFont(10,256,0, p10font);
    if(font_height == 15) VgaSetFont(15,256,0, p15font);
    VgaDisableFontAccess();

    /* This script is, for the most part, copied from DOSBox. */

    {unsigned long seq = 0x60300; if(!is_9pix) seq|=0x10; if(is_half) seq|=0x80;
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

    double clock = 28322000.0;
    if(((misc_output >> 2) & 3) == 0) clock = 25175000.0;
    clock /= (is_9pix ? 9.0 : 8.0);
    clock /= vtotal;
    clock /= htotal;
    if(is_half)   clock /= 2.0;
    VidFPS = clock;

#endif

    if(FatMode)
        width /= 2;
    if(C64palette)
    {
        memset(VidMem, 0x99, width*height*(FatMode?4:2));
        width -= 4; height -= 5;
    }

    VidCellHeight = font_height;
    VgaGetFont();
}
