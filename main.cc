#include <stdio.h>
#include <time.h>

#ifdef __BORLANDC__
# include <dos.h> // for MK_FP
# include <conio.h>
#else
# define cdecl
# define kbhit() 0
# define getch() 0
# define strnicmp strncasecmp
#endif

#include "vec_c.hh"
#include "vec_cp.hh"

#include "jsf.hh"

#ifdef __BORLANDC__
unsigned short* VidMem = (unsigned short *) MK_FP(0xB800, 0x0000);
#else
unsigned short VidMem[132*43];
#endif

unsigned char VidW=80, VidH=25, VidCellHeight=16;

CharPtrVecType EditLines;
CharPtrVecType EditColors;

JSF     Syntax;
size_t  WinX, WinY;
size_t  CurX, CurY;


size_t BlockBeginX=0, BlockBeginY=0;
size_t BlockEndX=0,   BlockEndY=0;

int     InsertMode=1, WaitingCtrl=0;
unsigned TabSize  =4;

void FileLoad(char* fn)
{
    fprintf(stderr, "Loading '%s'...\n", fn);
    FILE* fp = fopen(fn, "rb");
    if(!fp) { perror(fn); return; }
    fseek(fp, 0, SEEK_END);
    rewind(fp);

    EditLines.clear();
    EditColors.clear();

    int hadnl = 1;
    CharVecType editline;
    for(;;)
    {
        unsigned char Buf[512];
        size_t r = fread(Buf, 1, sizeof(Buf), fp);
        if(r == 0) break;
        for(size_t a=0; a<r; ++a)
        {
            if(Buf[a] == '\r' && Buf[a+1] == '\n') continue;
            if(Buf[a] == '\r') Buf[a] = '\n';
            if(Buf[a] == '\t')
            {
                do editline.push_back( ' ' );
                while(editline.size() % TabSize);
            }
            else
                editline.push_back( Buf[a] );

            hadnl = 0;
            if(Buf[a] == '\n')
            {
                EditLines.push_back(editline);
                CharVecType colorline( editline.size(), 0x24 );
                EditColors.push_back(colorline);
                editline.clear();
                hadnl = 1;
            }
        }
    }
    if(hadnl)
    {
        EditLines.push_back(editline);
        CharVecType colorline( editline.size(), 0x24 );
        EditColors.push_back(colorline);
        editline.clear();
    }
    fclose(fp);
    WinX = WinY = 0;
    CurX = CurY = 0;
}
void FileNew()
{
    WinX = WinY = 0;
    CurX = CurY = 0;
    EditLines.clear();
    EditColors.clear();
}
struct ApplyEngine: public JSF::Applier
{
    size_t x,y;
    ApplyEngine() : x(0),y(0) { }
    virtual cdecl int Get(void)
    {
        if(y >= EditLines.size() || EditLines[y].empty()) return -1;
        int ret = EditLines[y][x++];
        if(x == EditLines[y].size()) { x=0; ++y; }
        //fprintf(stdout, "Gets '%c'\n", ret);
        return ret;
    }
    virtual cdecl void Recolor(register unsigned n, register unsigned attr)
    {
        //fprintf(stdout, "Recolors %u as %02X\n", n, attr);
        size_t px=x, py=y;
        for(; n > 0; --n)
        {
            if(px == 0) { if(!py) break; --py; px = EditColors[py].size()-1; }
            else --px;
            EditColors[py][px] = attr;
        }
    }
};
void FileColorize()
{
    fprintf(stderr, "Applying syntax color...\n");
    JSF::ApplyState state;
    Syntax.ApplyInit(state);
    ApplyEngine eng;
    Syntax.Apply(state, eng);
}

void VisGetGeometry()
{
#ifdef __BORLANDC__
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl; mov VidCellHeight, cl }
    if(VidH == 0) VidH = 25; else VidH += 1;
#endif
}
void VisSetCursor()
{
#ifdef __BORLANDC__
    unsigned cx = WinX > CurX ? 0 : CurX-WinX;       if(cx >= VidW) cx = VidW-1;
    unsigned cy = WinY > CurY ? 1 : CurY-WinY; ++cy; if(cy >= VidH) cy = VidH-1;
    unsigned char cux = cx, cuy = cy;
    unsigned size = InsertMode ? (VidCellHeight-2) : (VidCellHeight*2/8);
    size = (size << 8) | (VidCellHeight-1);
    _asm { mov ah, 2; mov bh, 0; mov dh, cuy; mov dl, cux; int 0x10 }
    _asm { mov ah, 1; mov cx, size; int 0x10 }
#endif
}
void VisRenderStatus()
{
    unsigned short* Tgt = VidMem;

    char Buf[64];
    time_t t = time(0);
    struct tm* tm = localtime(&t);
    sprintf(Buf, "Row %-5uCol %-5u%02d:%02d:%02d",
        CurY+1,CurX+1, tm->tm_hour,tm->tm_min,tm->tm_sec);
    unsigned x0 = VidW-28, x1 = x0 + strlen(Buf);

    static const char Bayer[16] = {0,8,4,12,2,10,6,14, 3,11,7,15,1,9,5,13};
    static const unsigned short bgopt[4] = {0x3020, 0x37DC, 0x37DF, 0x7020};
    for(unsigned x=0; x<VidW; ++x)
    {
        double xscale = 16.0 * (1.0 - x / double(VidW-1));
        int c1 = Bayer[(x&15)] < xscale, c2 = Bayer[(x+1)&15] < xscale;
        unsigned color = bgopt[c1*2 + c2];
        if(x == 0 && WaitingCtrl)
            color = 0x7000 | '^';
        else if(x == 1 && WaitingCtrl)
            color = 0x7000 | WaitingCtrl;
        else if(x == 4 && InsertMode)
            color = 0x7000 | 'I';
        else if(x >= x0 && x < x1 && Buf[x-x0] != ' ')
            color = 0x3000 | Buf[x-x0];
        Tgt[x] = color;
    }
}
void VisRender()
{
    CharVecType EmptyLine;
    unsigned short* Tgt = VidMem + VidW;

    unsigned winh = VidH;
    for(unsigned y=0; y<winh; ++y)
    {
        unsigned ly = WinY + y;
        CharVecType* line  = &EmptyLine;
        CharVecType* color = &EmptyLine;
        if(ly < EditLines.size())
        {
            line  = &EditLines[ly];
            color = &EditColors[ly];
        }
        unsigned lw = line->size(), lx=0, x=WinX, xl=x + VidW;
        unsigned trail = 0x0720;
        for(unsigned l=0; l<lw; ++l)
        {
            unsigned char c = (*line)[l];
            /*if(c == 9)
                { lx += TabSize; lx -= (lx % TabSize); }
            else*/
            ++lx;
            if(lx > x)
            {
                unsigned attr = (*color)[l];
                if(c == '\n')
                    { c = ' '; trail = (attr << 8) | 0x20; }
                else if(c == 9)
                    { c = 0x1A; }
                attr = (attr << 8) | c;

                if( ((y == BlockBeginY && x >= BlockBeginX)
                  || y > BlockBeginY)
                &&  ((y == BlockEndY && x < BlockEndX)
                  || y < BlockEndY) )
                {
                    attr = (attr & 0x0FFF) | 0x1000;
                    if((attr >> 8) <= 0x11) attr = (attr & 0xFF) | 0x17;
                }

                unsigned attr1 = attr;
                if(c == 0x1A) { attr1 = (attr1 & 0xFF00) | 0x20; }

                do { *Tgt++ = attr; attr = attr1; } while(lx > ++x);
                if(x >= xl) break;
            }
        }
        while(x++ < xl)
            *Tgt++ = trail;
    }
}

static unsigned wx = ~0u, wy = ~0u, cx = ~0u, cy = ~0u;
static void WaitInput()
{
    if(cx != CurX || cy != CurY) { cx=CurX; cy=CurY; VisSetCursor(); }
    if(!kbhit())
    {
        while(CurX < WinX) WinX -= 8;
        while(CurX >= WinX + VidW) WinX += 8;
        if(wx != WinX || wy != WinY) { wx=WinX; wy=WinY; VisRender(); VisSetCursor(); }
        do {
            VisRenderStatus();
        #ifdef __BORLANDC__
            _asm { hlt }
        #endif
        } while(!kbhit());
    }
}

int main()
{
    Syntax.Parse("c.jsf");
    FileLoad("sample.cpp");
    FileColorize();
    fprintf(stderr, "Beginning render\n");
    VisGetGeometry();
    VisSetCursor();

    unsigned DimX = VidW, DimY = VidH-1;

    #define CTRL(c) ((c) & 0x1F)
    for(;;)
    {
        WaitInput();
        unsigned c = getch();
        switch(c)
        {
            case CTRL('V'): // ctrl-V
            {
            pgdn:;
                unsigned offset = CurY-WinY;
                CurY += DimY;
                if(CurY >= EditLines.size()) CurY = EditLines.size()-1;
                WinY = (CurY > offset) ? CurY-offset : 0;
                break;
            }
            case CTRL('U'): // ctrl-U
            {
            pgup:;
                unsigned offset = CurY - WinY;
                if(CurY > DimY) CurY -= DimY; else CurY = 0;
                WinY = (CurY > offset) ? CurY-offset : 0;
                break;
            }
            case CTRL('A'): goto home;
            case CTRL('E'): goto end;
            case CTRL('C'): // exit
            {
                /* TODO: Check if unsaved */
                goto exit;
            }
            case CTRL('K'):
            {
                WaitingCtrl='K';
                WaitInput();
                WaitingCtrl=0;
                switch(getch())
                {
                    case 'b': case 'B': case CTRL('B'): // mark begin
                        BlockBeginX = CurX; BlockBeginY = CurY;
                        VisRender();
                        break;
                    case 'k': case 'K': case CTRL('K'): // mark end
                        BlockEndX = CurX; BlockEndY = CurY;
                        VisRender();
                        break;
                    case 'm': case 'M': case CTRL('M'): // move block
                    case 'v': case 'V': case CTRL('V'): // paste block
                    case 'y': case 'Y': case CTRL('Y'): // delete block
                    case 'd': case 'D': case CTRL('D'): // save file
                    case 'x': case 'X': case CTRL('X'): // save and exit
                        ;
                }
                break;
            }
            case CTRL('Q'):
            {
                WaitingCtrl='';
                WaitInput();
                WaitingCtrl=0;
                break;
            }
            case 0:
                switch(getch())
                {
                    case 'H': // up
                        if(CurY > 0) --CurY;
                        if(CurY < WinY) WinY = CurY;
                        break;
                    case 'P': // down
                        if(CurY < EditLines.size()-1) ++CurY;
                        if(CurY >= WinY+DimY) WinY = CurY - DimY+1;
                        break;
                    case 'K': // left
                        if(CurX > 0) --CurX;
                        break;
                    case 'M': // right
                        ++CurX;
                        break;
                    case 0x47: // home
                    {
                    home:;
                        unsigned x = 0;
                        while(x < EditLines[CurY].size()
                           && EditLines[CurY][x] == ' ') ++x;
                        if(CurX == x) CurX = 0; else CurX = x;
                        break;
                    }
                    case 0x4F: // end
                    end: CurX = EditLines[CurY].size();
                        if(CurX > 0) --CurX; // past LF
                        break;
                    case 0x49: goto pgup;
                    case 0x51: goto pgdn;
                    case 0x52: // insert
                        InsertMode = !InsertMode;
                        VisSetCursor();
                        break;
                    case 0x84: // ctrl-pgup = goto beginning of file
                        CurY = WinY = 0;
                        CurX = WinX = 0;
                        break;
                    case 0x76: // ctrl-pgdn = goto end of file
                        CurY = EditLines.size()-1;
                        WinY = 0;
                        if(CurY >= WinY+DimY) WinY = CurY - DimY+1;
                        goto end;
                    case 0x77: // ctrl-home = goto beginning of window (vertically)
                        CurY = WinY;
                        break;
                    case 0x75: // ctrl-end = goto end of window (vertically)
                        CurY = WinY + VidH-1;
                        break;
                    case 0x53: // delete
                        ;
                }
        }
    }
exit:;
    CurX = 0; CurY = WinY + VidH; InsertMode = 1;
    VisSetCursor();
    return 0;
}
