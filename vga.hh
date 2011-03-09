#ifdef __BORLANDC__
unsigned short* VidMem = (unsigned short *) MK_FP(0xB800, 0x0000);
#else
//unsigned short VidMem[132*43];
#endif

unsigned char VidW=80, VidH=25, VidCellHeight=16;

unsigned char* VgaFont = 0;

void VgaGetFont()
{
    unsigned char mode = 1;
    switch(VidCellHeight)
    {
        case 8: mode = 3; break;
        case 14: mode = 2; break;
        case 16: mode = 6; break;
        default: mode = 1; break;
    }
    _asm { push es; push bp
           mov ax, 0x1130
           mov bh, mode
           int 0x10
           mov word ptr VgaFont+0, bp
           mov word ptr VgaFont+2, es
           pop bp
           pop es }
}

void VgaGetMode()
{
#ifdef __BORLANDC__
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl; mov VidCellHeight, cl }
    if(VidH == 0) VidH = 25; else VidH += 1;
    _asm { mov ax, 0x1003; xor bx,bx; int 0x10 } // Disable blink-bit
    VgaGetFont();
#endif
}

void VgaSetMode(unsigned modeno, int is_doublewidth)
{
    if(modeno < 0x100)
        _asm { mov ax, modeno; int 0x10 }
    else
        _asm { mov bx, modeno; mov ax, 0x4F02; int 0x10 }

    outport(0x3C4, is_doublewidth ? 0x0801 : 0x0101);
}

void VgaSetCustomMode(
    unsigned width,
    unsigned height,
    unsigned font_height,
    int is_9pix,
    int is_half/*horizontally doubled*/,
    int is_double/*vertically doubled*/)
{
    unsigned hdispend = width;
    unsigned vdispend = height*font_height;
    if(is_double) vdispend *= 2;
    unsigned htotal = width*5/4;
    unsigned vtotal = vdispend+45;

    // Set standard 80x25 mode as a baseline
    // This triggers font reset on JAINPUT.
    *(unsigned char*)MK_FP(0x40,0x87) |= 0x80;
    _asm { mov ax, 3; int 0x10 }
    *(unsigned char*)MK_FP(0x40,0x87) &= ~0x80;

    if(font_height ==14) { _asm { mov ax, 0x1101; mov bl, 0; int 0x10 } }
    if(font_height == 8) { _asm { mov ax, 0x1102; mov bl, 0; int 0x10 } }

    /* This script is, for the most part, copied from DOSBox. */

    {unsigned char Seq[5] = { 0, !is_9pix + 8*is_half, 3, 0, 6 };
    for(unsigned a=0; a<5; ++a) outport(0x3C4, a | (Seq[a] << 8));}

    // Disable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)&0x7F);
    // crtc
    {for(unsigned a=0; a<=0x18; ++a) outport(0x3D4, a | 0x0000);}
    unsigned overflow = 0, max_scanline = 0, ver_overflow = 0, hor_overflow = 0;
    outport(0x3D4, 0x00 | ((htotal-5) << 8));   hor_overflow |= ((htotal-5) >> 8) << 0;
    outport(0x3D4, 0x01 | ((hdispend-1) << 8)); hor_overflow |= ((hdispend-1) >> 8) << 1;
    outport(0x3D4, 0x02 | ((hdispend) << 8));   hor_overflow |= ((hdispend) >> 8) << 2;
    unsigned blank_end = (htotal-2) & 0x7F;
    outport(0x3D4, 0x03 | ((0x80|blank_end) << 8));
    unsigned ret_start = is_half ? hdispend+3 : hdispend+5;
    outport(0x3D4, 0x04 | (ret_start << 8));    hor_overflow |= ((ret_start) >> 8) << 4;
    unsigned ret_end = is_half ? ((htotal-18)&0x1F)|(is_double?0x20:0) : ((htotal-4)&0x1F);
    outport(0x3D4, 0x05 | ((ret_end | ((blank_end & 0x20) << 2)) << 8));
    outport(0x3D4, 0x06 | ((vtotal-2) << 8));
    overflow |= ((vtotal-2) & 0x100) >> 8;
    overflow |= ((vtotal-2) & 0x200) >> 4;
    ver_overflow |= (((vtotal-2) & 0x400) >> 10);
    unsigned vretrace = vdispend + (4800/vdispend + (vdispend==350?24:0));
    outport(0x3D4, 0x10 | (vretrace << 8));
    overflow |= (vretrace & 0x100) >> 6;
    overflow |= (vretrace & 0x200) >> 2;
    ver_overflow |= (vretrace & 0x400) >> 6;
    outport(0x3D4, 0x11 | (((vretrace+2) & 0xF) << 8));
    outport(0x3D4, 0x12 | ((vdispend-1) << 8));
    overflow |= ((vdispend-1) & 0x100) >> 7;
    overflow |= ((vdispend-1) & 0x200) >> 3;
    ver_overflow |= ((vdispend-1) & 0x400) >> 9;
    unsigned vblank_trim = vdispend / 66;
    outport(0x3D4, 0x15 | ((vdispend+vblank_trim) << 8));
    overflow |= ((vdispend+vblank_trim) & 0x100) >> 5;
    max_scanline |= ((vdispend+vblank_trim) & 0x200) >> 4;
    ver_overflow |= ((vdispend+vblank_trim) & 0x400) >> 8;
    outport(0x3D4, 0x16 | ((vtotal-vblank_trim-2) << 8));
    unsigned line_compare = vtotal < 1024 ? 1023 : 2047;
    outport(0x3D4, 0x18 | (line_compare << 8));
    overflow |= (line_compare & 0x100) >> 4;
    max_scanline |= (line_compare & 0x200) >> 3;
    ver_overflow |= (line_compare & 0x400) >> 4;
    if(is_double) max_scanline |= 0x80;
    max_scanline |= font_height-1;
    unsigned underline = vdispend == 350 ? 0x0F : 0x1F;
    outport(0x3D4, 0x09 | (max_scanline << 8));
    outport(0x3D4, 0x14 | (underline << 8));
    outport(0x3D4, 0x07 | (overflow << 8));
    outport(0x3D4, 0x5D | (hor_overflow << 8)); // S3 trio extension
    outport(0x3D4, 0x5E | (ver_overflow << 8)); // S3 trio extension
    unsigned offset = hdispend / 2;
    outport(0x3D4, 0x13 | (offset << 8));
    outport(0x3D4, 0x51 | (((offset & 0x300) >> 4) << 8));
    outport(0x3D4, 0x69 | (0 << 8)); // S3 trio
    outport(0x3D4, 0x5E | (ver_overflow << 8)); // (repeat for some reason)
    unsigned modecontrol = 0xA3;
    outport(0x3D4, 0x17 | (modecontrol << 8));
    // Enable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)|0x80);
    outport(0x3D4, 0x67 | (0 << 8)); // S3 trio

    outportb(0x3C2, 0x63/*0x02*/); // misc output

    {unsigned char Gfx[9] = { 0,0,0,0,0,0x10,0x0E,0x0F,0xFF };
    for(unsigned a=0; a<9; ++a) outport(0x3CE, a | (Gfx[a] << 8));}

    {unsigned char Att[0x15] = { 0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
                                 0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
                                is_9pix*4, 0, 0x0F, is_9pix*8, 0 };
    _asm { mov dx,0x3DA; in al,dx }
    {for(unsigned a=0x10; a<0x15; ++a)
        { if(a == 0x11) continue;
          outportb(0x3C0, a);
          outportb(0x3C0, Att[a]); }} }
    _asm { mov dx,0x3DA; in al,dx }
    outportb(0x3C0, 0x20);

    *(unsigned char*)MK_FP(0x40,0x10) &= ~0x30;
    *(unsigned char*)MK_FP(0x40,0x10) |= 0x20;

    *(unsigned char*)MK_FP(0x40,0x65) = 0x29;
    *(unsigned char*)MK_FP(0x40, 0x4A) = width;
    *(unsigned char*)MK_FP(0x40, 0x84) = height-1;
    *(unsigned char*)MK_FP(0x40, 0x85) = font_height;
    *(unsigned short*)MK_FP(0x40, 0x4C) = width*height*2;
    VidW = width;
    VidH = height;
    VidCellHeight = font_height;
    VgaGetFont();
}
