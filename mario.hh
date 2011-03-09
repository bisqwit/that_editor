#include <string.h>

static const char Mario[] =
//     0123456789ABCDEF        0123456789ABCDEF        0123456789ABCDEF
//12345""""""""""""""""012345________________012345________________012345
/*
"??????      #####     ??????                ??????      #####     ??????"
"??????     #########  ??????       #####    ??????     #########  ??????"
"??????     %%%''%'    ??????      ######### ??????     %%%''%'    ??????"
"??????    %'%'''%'''  ??????      %%%''%'   ??????    %'%'''%'''  ??????"
"??????    %'%%'''%''' ??????     %'%'''%''' ??????    %'%%'''%''' ??????"
"??????    %%''''%%%%  ??????     %'%%'''%'' ??????    %%''''%%%%  ??????"
"??????      '''''''   ??????     %%''''%%%% ??????      '''''''   ??????"
"??????     %%#%%%     ??????       '''''''  ??????   %%%%##%%     ??????"
"??????    %%%%##%%    ??????      %%%%#% '  ?????? ''%%%%###%%%'''??????"
"??????    %%%##'##'   ??????     '%%%%%%''' ?????? ''' %%#'###%%''??????"
"??????    %%%%#####   ??????    ''#%%%%%''  ?????? ''  #######  % ??????"
"??????    #%%'''###   ??????    %%#######   ??????    #########%% ??????"
"??????     #%''###    ??????    %########   ??????   ##########%% ??????"
"??????      ###%%%    ??????   %%### ###    ??????  %%###   ###%% ??????"
"??????      %%%%%%%   ??????   %    %%%     ??????  %%%           ??????"
"??????      %%%%      ??????        %%%%    ??????   %%%          ??????"*/

//.....0123456789ABCDEF      0123456789ABCDEF  
//12345""""""""""""""""012345""""""""""""""""012345
"??????                ??????      #####     ??????"
"??????      ######    ??????     #'''''###  ??????"
"??????     #''''''##  ??????    #'''''''''# ??????"
"??????    #'''''''''# ??????    ###'.#.###  ??????"
"??????    ###..#.###  ??????   #..##.#....# ??????"
"??????   #..##.#....# ??????   #..##..#...# ??????"
"??????   #..##..#...# ??????    ##...#####  ??????"
"??????    ##...#####  ??????    ###.....#   ??????"
"??????     ##.....#   ??????  ##'''##''###  ??????"
"??????    #''##''#    ?????? #..''''##''#'# ??????"
"??????   #''''##''#   ?????? #..'''######'.#??????"
"??????   #''''#####   ??????  #..####.##.#.#??????"
"??????    #...##.##   ??????  .#########''# ??????"
"??????    #..'''###   ??????  #''######'''# ??????"
"??????     #'''''#    ??????  #'''#  #'''#  ??????"
"??????      #####     ??????   ###    ###   ??????";
const unsigned NumMarioPoses = 2;
const unsigned MarioStepInterval = 7;
const unsigned MarioColor = 0x0800;


static unsigned OverlayMarioByte(
    unsigned pose,
    register unsigned y,
    register unsigned char OriginalByte,
    signed int MarioOffset)
{
    const char* data =
        Mario + 6 + pose * unsigned(16+6)
      + y * unsigned(6*(NumMarioPoses+1)+16*NumMarioPoses)
      + MarioOffset;

    static const unsigned char dither4x4[16] =
        {0,12,3,15, 8,4,11,7, 2,14,1,13, 10,6,9,5};
    const unsigned char* dithline = dither4x4 + ((y & 3u) * 4u);

    unsigned char byte = 0x00;
    for(register unsigned x=0; x<8; ++x)
    {
        unsigned char dithval = dithline[x&3];
        register unsigned char bit = 1;
        switch(*data++ / 3)
        {
            case '.' / 3: // 15
                bit = 0;            // 0%
                break;
            case '\'' / 3: // 13
                bit = dithval >> 3; // 50%
                break;
            case '%' / 3: // 12
                bit = dithval >= 4; // 75%
                break;
            case '#' / 3: // 11
                break;              // 100%
            default:
            case '?' / 3: // 21
            case ' ' / 3: // 10
                // transparent, no change
                byte |= OriginalByte & (0x80 >> x);
                continue;
        }
        if(bit)
            byte |= 1u << (7-x);
    }
    return byte;
}

unsigned long MarioTimer = 0;
void MarioTranslate(
    unsigned short* model,
    unsigned short* target,
    unsigned width)
{
    const unsigned room_left   = 240;
    const unsigned room_right  = 8;
    const unsigned room_wide   = width*8;
    const unsigned xspanlength = room_wide + room_left + room_right;
    //const unsigned twospans = xspanlength * 2u;
    unsigned long mt = MarioTimer / 1;
    unsigned marioframe = (mt / MarioStepInterval) % 2u;
    /*
    unsigned mariopos = timer % twospans;
    int mariox = mariopos < xspanlength
        ? (int)mariopos - (int)room_left
        : (int)room_wide + (int)room_right - (int)(mariopos - xspanlength);
    */
    int mariox = (mt % xspanlength) - room_left;

    unsigned char RevisedFontData[ 3 * 32 ];

    static const unsigned short chartable[3] =
        {0xD9 + MarioColor,
         0xDA + MarioColor,
         0xDF + MarioColor};
    unsigned fontdatasize  = 0;
    unsigned numchars      = 0;
    for(int basex = mariox - (mariox % 8)
      ; basex < mariox + 16
     && basex < room_wide
      ; basex += 8)
    {
        if(basex < 0) continue;

        int offset = basex - mariox;
        unsigned short ch = model[basex >> 3];
        if( (ch & 0xFF) == 0xDC )
            ch = (ch & 0xFF00u) | 0x20;

        const unsigned char* SourceFontPtr = VgaFont + ((ch & 0xFF) * VidCellHeight);

        for(unsigned y=0; y<VidCellHeight; ++y)
        {
            RevisedFontData[fontdatasize++] =
                OverlayMarioByte(marioframe,y, SourceFontPtr[y], offset);
        }
        model[basex >> 3] = (ch & 0xF000u) | chartable[numchars++];
    }

    if(numchars > 0)
    {
        // Update VGA font:
        _asm {
            push es
            push bp
             mov ax, 0x1100
             mov bl, 0
             mov bh, VidCellHeight
             mov dx, 0xD9
             mov cx, 2
             push ss
             pop es
             lea bp, offset RevisedFontData
             int 0x10
             xchg bl,bh
              add bp, bx
              add bp, bx
             xchg bl,bh
             mov dx, 0xDF
             mov cl, 1
             int 0x10
            pop bp
            pop es
        }
    }
    memcpy(target, model, width*2);
}

static unsigned rate=60U, Clock=0u, Counter=0x1234DCUL/rate;
static void (interrupt *OldI08)();
static void interrupt MarioI08()
{
    ++MarioTimer;
    _asm {
        mov ax, Counter
        add Clock, ax
        jnc P1
    }
    OldI08();
    goto P2;
P1: _asm { mov al, 0x20; out 0x20, al }
P2:;
}

void InstallMario()
{
    disable();
    (void *)OldI08 = *(void **)MK_FP(0, 8*4);
    *(void **)MK_FP(0, 8*4) = (void *)MarioI08;
    _asm { mov al, 0x34;    out 0x43, al
           mov ax, Counter; out 0x40, al
           mov al, ah;      out 0x40, al }
    enable();
}

void DeInstallMario()
{
    disable();
    *(void **)MK_FP(0, 8*4) = (void *)OldI08;
    _asm { mov al, 0x34;    out 0x43, al
           xor al, al;      out 0x40, al
           /*********/      out 0x40, al }
    enable();
}
