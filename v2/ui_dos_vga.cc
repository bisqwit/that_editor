};
#include <cstdint>

#include "ui.hh"
#include "djgpp-memaccess.hh"

static const uint_least32_t c64pal[16] =
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
static const uint_least32_t egapal[16] =
{
    0x000000ul,
    0x0000AAul,
    0x00AA00ul,
    0x00AAAAul,
    0xAA0000ul, // 4
    0xAA00AAul,
    0xAA5500ul,
    0xAAAAAAul,
    0x555555ul, // 8
    0x5555FFul,
    0x55FF55ul,
    0x55FFFFul,
    0xFF5555ul, // C
    0xFF55FFul,
    0xFFFF55ul,
    0xFFFFFFul
};

static const struct AvailableFonts[] =
{
    { 8,8   },
    { 9,8   },
    { 8,14  },
    { 9,14  },
    { 8,16  },
    { 9,16  },
    { 8,19  },
    { 9,19  },
    { 8,32  },
    { 9,32  },
    { 16,32 }
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

static void VgaSetMode(unsigned modeno)
{
    __dpmi_regs regs = { };
    if(modeno < 0x100)
        regs.x.ax = modeno;
    else
    {
        regs.x.ax = 0x4F02;
        regs.x.bx = /*0x4000 |*/ modeno;
    }
    __dpmi_int(0x10, &regs);

    /*
    // Map the video memory into memory
    __dpmi_meminfo meminfo;
    meminfo.address = 0xC0000000ul; // DOSBox's S3 video memory
    meminfo.size    = PC::W * PC::H * 4;
    __dpmi_physical_address_mapping(&meminfo);
    __dpmi_lock_linear_region(&meminfo);
    selector = __dpmi_allocate_ldt_descriptors(1);
    __dpmi_set_segment_base_address(selector, meminfo.address);
    __dpmi_set_segment_limit(selector, ((meminfo.size+4095)&~4095)-1);
    */

    outportb(0x3C8, 0x20);
    for(unsigned a=0; a<16; ++a)
    {
        outportb(0x3C9, c64pal[a] >> 18);
        outportb(0x3C9, (c64pal[a] >> 10) & 0x3F);
        outportb(0x3C9, (c64pal[a] >> 2) & 0x3F);
    }
}

#include "ui_fonts.inc"

static void VgaSetFont
   (const unsigned char* data,
    unsigned firstchar, unsigned nchars,
    unsigned Height)
{
    outportw(0x3C4,0x2 | (4<<8)); // Enable plane 2
    outportb(0x3CE,0x6);
    unsigned char old6 = inportb(0x3CF);
    outportb(0x3CF,0); // Disable odd/even and A000 addressing

    dosmemput(data, nchars*Height, 0xA000 + firstchar*Height);

    outportw(0x3C4,0x2 | (3<<8)); // Enable textmode planes 0,1)
    outportw(0x3CE,0x6 | (old6<<8)); // Enable odd/even and B800 addressing
}

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
    bool FatMode = false;
    bool is_9pix = false;
    if(font_width >= 16) { FatMode = true; font_width /= 2; }
    if(font_width == 9)  { is_9pix = true; font_width = 8; }

    unsigned hdispend = width;
    unsigned vdispend = height * font_height;
    if(font_y_double) vdispend *= 2;
    unsigned htotal = width   * 5 / 4;
    unsigned vtotal = vdispend + 45;

    if(font_height == 16)
    {
        // Set standard 80x25 mode as a baseline
        // This triggers font reset on JAINPUT.
        MemByteProxy{0x487} |= 0x80; // tell BIOS to not clear VRAM
        VgaSetMode(3);
        MemByteProxy{0x487} &= (unsigned char)~0x80;
    }
    switch(font_height)
    {
        case  8:
            if(C64mode)
                VgaSetFont(c64font, 32, 256-64, 8);
            else
                VgaSetFont( p8font, 0, 256, 8);
            break;
        case 14: VgaSetFont(p14font, 0, 256, 14); break;
        case 16: VgaSetFont(p16font, 0, 256, 16); break;
        case 19: VgaSetFont(p19font, 0, 256, 19); break;
        case 32:
            if(font_width == 8)
                VgaSetFont(p32font, 0,256, 32);
            else
                VgaSetFont(p32wfont, 0,256, 32);
            break;
    }
    /* This script is, for the most part, copied from DOSBox. */

    {unsigned char Seq[5] = { 0, !is_9pix + 8*font_x_double, 3, 0, 6 };
    for(unsigned a=0; a<5; ++a) outportw(0x3C4, a | (Seq[a] << 8));}

    // Disable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)&0x7F);
    // crtc
    {for(unsigned a=0; a<=0x18; ++a) outportw(0x3D4, a | 0x0000);}
    unsigned overflow = 0, max_scanline = 0, ver_overflow = 0, hor_overflow = 0;
    outportw(0x3D4, 0x00 | ((htotal-5) << 8));   hor_overflow |= ((htotal-5) >> 8) << 0;
    outportw(0x3D4, 0x01 | ((hdispend-1) << 8)); hor_overflow |= ((hdispend-1) >> 8) << 1;
    outportw(0x3D4, 0x02 | ((hdispend) << 8));   hor_overflow |= ((hdispend) >> 8) << 2;
    unsigned blank_end = (htotal-2) & 0x7F;
    outportw(0x3D4, 0x03 | ((0x80|blank_end) << 8));
    unsigned ret_start = font_x_double ? hdispend+3 : hdispend+5;
    outportw(0x3D4, 0x04 | (ret_start << 8));    hor_overflow |= ((ret_start) >> 8) << 4;
    unsigned ret_end = font_x_double ? ((htotal-18)&0x1F)|(font_y_double?0x20:0) : ((htotal-4)&0x1F);
    outportw(0x3D4, 0x05 | ((ret_end | ((blank_end & 0x20) << 2)) << 8));
    outportw(0x3D4, 0x06 | ((vtotal-2) << 8));
    overflow |= ((vtotal-2) & 0x100) >> 8;
    overflow |= ((vtotal-2) & 0x200) >> 4;
    ver_overflow |= (((vtotal-2) & 0x400) >> 10);
    unsigned vretrace = vdispend + (4800/vdispend + (vdispend==350?24:0));
    outportw(0x3D4, 0x10 | (vretrace << 8));
    overflow |= (vretrace & 0x100) >> 6;
    overflow |= (vretrace & 0x200) >> 2;
    ver_overflow |= (vretrace & 0x400) >> 6;
    outportw(0x3D4, 0x11 | (((vretrace+2) & 0xF) << 8));
    outportw(0x3D4, 0x12 | ((vdispend-1) << 8));
    overflow |= ((vdispend-1) & 0x100) >> 7;
    overflow |= ((vdispend-1) & 0x200) >> 3;
    ver_overflow |= ((vdispend-1) & 0x400) >> 9;
    unsigned vblank_trim = vdispend / 66;
    outportw(0x3D4, 0x15 | ((vdispend+vblank_trim) << 8));
    overflow |= ((vdispend+vblank_trim) & 0x100) >> 5;
    max_scanline |= ((vdispend+vblank_trim) & 0x200) >> 4;
    ver_overflow |= ((vdispend+vblank_trim) & 0x400) >> 8;
    outportw(0x3D4, 0x16 | ((vtotal-vblank_trim-2) << 8));
    unsigned line_compare = vtotal < 1024 ? 1023 : 2047;
    outportw(0x3D4, 0x18 | (line_compare << 8));
    overflow |= (line_compare & 0x100) >> 4;
    max_scanline |= (line_compare & 0x200) >> 3;
    ver_overflow |= (line_compare & 0x400) >> 4;
    if(font_y_double) max_scanline |= 0x80;
    max_scanline |= font_height-1;
    unsigned underline = 0x1F; //vdispend == 350 ? 0x0F : 0x1F;
    outportw(0x3D4, 0x09 | (max_scanline << 8));
    outportw(0x3D4, 0x14 | (underline << 8));
    outportw(0x3D4, 0x07 | (overflow << 8));
    outportw(0x3D4, 0x5D | (hor_overflow << 8)); // S3 trio extension
    outportw(0x3D4, 0x5E | (ver_overflow << 8)); // S3 trio extension
    unsigned offset = hdispend / 2;
    outportw(0x3D4, 0x13 | (offset << 8));
    outportw(0x3D4, 0x51 | (((offset & 0x300) >> 4) << 8));
    outportw(0x3D4, 0x69 | (0 << 8)); // S3 trio
    outportw(0x3D4, 0x5E | (ver_overflow << 8)); // (repeat for some reason)
    unsigned modecontrol = 0xA3;
    outportw(0x3D4, 0x17 | (modecontrol << 8));
    // Enable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)|0x80);
    outportw(0x3D4, 0x67 | (0 << 8)); // S3 trio

    unsigned misc_output = 0x63;/*0x02*/
    outportb(0x3C2, misc_output); // misc output

    {unsigned char Gfx[9] = { 0,0,0,0,0,0x10,0x0E,0x0F,0xFF };
    for(unsigned a=0; a<9; ++a) outportw(0x3CE, a | (Gfx[a] << 8));}

    {unsigned char Att[0x15] = { 0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
                                 0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
                                 is_9pix*4, 0, 0x0F, is_9pix*8, 0 };
    if(C64mode)
    {
        for(unsigned a=0; a<0x10; ++a) Att[a] = 0x20+a;
    }

    inportb(0x3DA);//_asm { mov dx,0x3DA; in al,dx }
    {for(unsigned a=0x0; a<0x15; ++a)
        { if(a == 0x11) continue;
          outportb(0x3C0, a);
          outportb(0x3C0, Att[a]); }} }
    inportb(0x3DA);//_asm { mov dx,0x3DA; in al,dx }
    outportb(0x3C0, 0x20);

    MemByteProxy{0x410} &= ~0x30;
    MemByteProxy{0x410} |= 0x20;
    MemByteProxy{0x465} = 0x29;
    MemByteProxy{0x44A} = width;
    MemByteProxy{0x484} = height-1;
    MemByteProxy{0x485} = font_height;
    MemWordProxy{0x44C} = width*height*2;

    double clock = 28322000.0;
    if(((misc_output >> 2) & 3) == 0) clock = 25175000.0;
    clock /= (is_9pix ? 9.0 : 8.0);
    clock /= vtotal;
    clock /= htotal;
    if(font_x_double)   clock /= 2.0;
    VidFPS = clock;

    if(FatMode)
        width /= 2;
    if(C64mode)
    {
        for(unsigned b=width*height*(FatMode?2:1), a=0; a<b; ++a)
            VidMem[a] = 0x9999;

        width -= 4; height -= 5;
    }

    VidW = width;
    VidH = height;
    VidCellHeight = font_height;
}
