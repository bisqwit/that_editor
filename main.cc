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
char StatusLine[256] =
"Ad-hoc programming editor - (C) 2011-03-08 Joel Yliluoma";

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
                size_t nextstop = editline.size() + TabSize;
                nextstop -= nextstop % TabSize;
                editline.resize(nextstop, ' ');
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
    int finished;
    size_t x,y;
    ApplyEngine() : x(0),y(0), finished(0) { }
    void Reset() { x=y=0; finished=0; }
    virtual cdecl int Get(void)
    {
        if(kbhit()) return -1;
        if(y >= EditLines.size() || EditLines[y].empty())
        {
            finished = 1;
            return -1;
        }
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

void VisGetGeometry()
{
#ifdef __BORLANDC__
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl; mov VidCellHeight, cl }
    if(VidH == 0) VidH = 25; else VidH += 1;
    // Disable blink-bit
    _asm {
        mov dx,0x3DA; in al,dx
        mov al,0x30; mov dl,0xC0;out dx,al; nop
        xor al,al; out dx,al
        mov dl,0xDA; in al,dx }
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
    unsigned short* Hdr = VidMem;
    unsigned short* Stat = VidMem + VidW * (VidH-1);

    time_t t = time(0);
    struct tm* tm = localtime(&t);

    char Buf1[80], Buf2[80];
    sprintf(Buf1, "Row %-5uCol %-5u", CurY+1,CurX+1);
    sprintf(Buf2, "%02d:%02d:%02d", tm->tm_hour,tm->tm_min,tm->tm_sec);
    unsigned x1a = 20,         x1b = x1a + strlen(Buf1);
    unsigned x2a = VidW*55/70, x2b = x2a + strlen(Buf2);

    static const unsigned char slide[] = {3,7,7,7,7,3,3,3,8};
    static const unsigned char slide2[] = {15,15,15,14,7,6,8,0,0};
    #define f(v) v / 16.0f
    static const float Bayer[16] = { f(0),f(8),f(4),f(12),f(2),f(10),f(6),f(14),
                                     f(3),f(11),f(7),f(15),f(1),f(9),f(5),f(13) };
    #undef f
    {for(unsigned x=0; x<VidW; ++x)
    {
        float xscale = x * (sizeof(slide)-1) / float(VidW-1);
        unsigned c1 = slide[(unsigned)( xscale + Bayer[0*8 + (x&7)] )];
        unsigned c2 = slide[(unsigned)( xscale + Bayer[1*8 + (x&7)] )];
        unsigned color = (c1 << 12u) | (c2 << 8u) | 0xDC;

        if(x == 0 && WaitingCtrl)
            color = (color&0xF000) | '^';
        else if(x == 1 && WaitingCtrl)
            color = (color&0xF000) | WaitingCtrl;
        else if(x == 4 && InsertMode)
            color = (color&0xF000) | 'I';
        else if(x >= x1a && x < x1b && Buf1[x-x1a] != ' ')
            color = (color&0xF000) | Buf1[x-x1a];
        else if(x >= x2a && x < x2b && Buf2[x-x2a] != ' ')
            color = (color&0xF000) | Buf2[x-x2a];
        Hdr[x] = color;
    }}
    if(StatusLine[0])
        {for(unsigned p=0,x=0; x<VidW; ++x)
        {
            float xscale = x * (sizeof(slide2)-1) / float(VidW-1);
            unsigned c1 = slide2[(unsigned)( xscale + Bayer[0*8 + (x&7)] )];
            unsigned c2 = slide2[(unsigned)( xscale + Bayer[1*8 + (x&7)] )];
            unsigned color = (c1 << 12u) | (c2 << 8u) | 0xDC;
            unsigned char c = StatusLine[p]; if(!c) c = 0x20;
            if(c != 0x20) color = (color & 0xF000u) | c;
            Stat[x] = color;
            if(StatusLine[p]) ++p;
        }}
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
            if(c == '\n') break;
            ++lx;
            if(lx > x)
            {
                unsigned attr = (*color)[l];
                attr = (attr << 8) | c;

                if( ((ly == BlockBeginY && lx-1 >= BlockBeginX)
                  || ly > BlockBeginY)
                &&  ((ly == BlockEndY && lx-1 < BlockEndX)
                  || ly < BlockEndY) )
                {
                    attr = (attr & 0xFFu)
                         | ((attr >> 4u) & 0xF00u)
                         | ((attr << 4u) & 0xF000u);
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

unsigned wx = ~0u, wy = ~0u, cx = ~0u, cy = ~0u;

int             SyntaxCheckingNeeded = 1;
JSF::ApplyState SyntaxCheckingState;
ApplyEngine     SyntaxCheckingApplier;
void WaitInput()
{
    if(cx != CurX || cy != CurY) { cx=CurX; cy=CurY; VisSetCursor(); }
    if(StatusLine[0] && CurY >= WinY+VidH-2) WinY += 2;
    if(!kbhit())
    {
        while(CurX < WinX) WinX -= 8;
        while(CurX >= WinX + VidW) WinX += 8;
        if(SyntaxCheckingNeeded)
        {
            if(SyntaxCheckingNeeded != -1)
            {
                Syntax.ApplyInit(SyntaxCheckingState);
                SyntaxCheckingApplier.Reset();
            }
            Syntax.Apply(SyntaxCheckingState, SyntaxCheckingApplier);
            if(SyntaxCheckingApplier.finished)
                SyntaxCheckingNeeded = 0;
            else
                SyntaxCheckingNeeded = -1;
            VisRender();
        }
        if(wx != WinX || wy != WinY) { wx=WinX; wy=WinY; VisRender(); VisSetCursor(); }
        do {
            VisRenderStatus();
        #ifdef __BORLANDC__
            _asm { hlt }
        #endif
        } while(!kbhit());
    }
}

void BlockIndent(int offset)
{
    unsigned firsty = BlockBeginY, lasty = BlockEndY;
    if(BlockEndX == 0) lasty -= 1;

    unsigned min_indent = 0x7FFF, max_indent = 0;
    for(unsigned y=firsty; y<=lasty; ++y)
    {
        unsigned indent = 0;
        while(indent < EditLines[y].size()
           && EditLines[y][indent] == ' ') ++indent;
        if(EditLines[y][indent] == '\n') continue;
        if(indent < min_indent) min_indent = indent;
        if(indent > max_indent) max_indent = indent;
    }
    if(offset > 0)
    {
        CharVecType indentbuf(offset, ' ');
        CharVecType colorbuf(offset, 0x07);
        for(unsigned y=firsty; y<=lasty; ++y)
        {
            unsigned indent = 0;
            while(indent < EditLines[y].size()
               && EditLines[y][indent] == ' ') ++indent;
            if(EditLines[y][indent] == '\n') continue;

            EditLines[y].insert(EditLines[y].begin(),
                indentbuf.begin(),
                indentbuf.end());
            EditColors[y].insert(EditColors[y].begin(),
                colorbuf.begin(),
                colorbuf.end());
        }
        if(BlockBeginX > 0) BlockBeginX += offset;
        if(BlockEndX   > 0) BlockEndX   += offset;
    }
    else if(min_indent >= -offset)
    {
        unsigned outdent = -offset;
        for(unsigned y=firsty; y<=lasty; ++y)
        {
            unsigned indent = 0;
            while(indent < EditLines[y].size()
               && EditLines[y][indent] == ' ') ++indent;
            if(EditLines[y][indent] == '\n') continue;
            if(indent < outdent) continue;
            EditLines[y].erase(
                EditLines[y].begin(),
                EditLines[y].begin() + outdent);
            EditColors[y].erase(
                EditColors[y].begin(),
                EditColors[y].begin() + outdent);
        }
        if(BlockBeginX >= outdent) BlockBeginX -= outdent;
        if(BlockEndX   >= outdent) BlockEndX   -= outdent;
    }
    SyntaxCheckingNeeded = 1;
}

void FindPair()
{
    unsigned char PairChar = 0;
    int           PairDir  = 0;
    unsigned char PairColor = EditColors[CurY][CurX];
    switch(EditLines[CurY][CurX])
    {
        case '{': PairChar = '}'; PairDir=1; break;
        case '[': PairChar = ']'; PairDir=1; break;
        case '(': PairChar = ')'; PairDir=1; break;
        case '}': PairChar = '{'; PairDir=-1; break;
        case ']': PairChar = '['; PairDir=-1; break;
        case ')': PairChar = '('; PairDir=-1; break;
    }
    if(!PairDir) return;
    int balance = 0;
    unsigned testx = CurX, testy = CurY;
    if(PairDir > 0)
        for(;;)
        {
            if(++testx >= EditLines[testy].size())
                { testx=0; ++testy; if(testy >= EditLines.size()) return; }
            if(EditColors[testy][testx] != PairColor) continue;
            unsigned char c = EditLines[testy][testx];
            if(balance == 0 && c == PairChar) { CurX = testx; CurY = testy; return; }
            if(c == '{' || c == '[' || c == '(') ++balance;
            if(c == '}' || c == ']' || c == ')') --balance;
        }
    else
        for(;;)
        {
            if(testx == 0)
                { if(testy == 0) return; testx = EditLines[--testy].size() - 1; }
            else
                --testx;
            if(EditColors[testy][testx] != PairColor) continue;
            unsigned char c = EditLines[testy][testx];
            if(balance == 0 && c == PairChar) { CurX = testx; CurY = testy; return; }
            if(c == '{' || c == '[' || c == '(') ++balance;
            if(c == '}' || c == ']' || c == ')') --balance;
        }
}

int main()
{
    Syntax.Parse("c.jsf");
    FileLoad("sample.cpp");
    fprintf(stderr, "Beginning render\n");
    VisGetGeometry();
    VisSetCursor();

    unsigned DimX = VidW, DimY = VidH-1;

    #define CTRL(c) ((c) & 0x1F)
    for(;;)
    {
        WaitInput();
        unsigned c = getch();
        int shift = 3 & (*(char*)MK_FP(0x40,0x17));
        int wasbegin = CurX==BlockBeginX && CurY==BlockBeginY;
        int wasend   = CurX==BlockEndX && CurY==BlockEndY;
        unsigned WasX = CurX, WasY = CurY;
        int dragalong=0, extch=0;
        if(StatusLine[0])
        {
            StatusLine[0] = '\0';
            VisRender();
        }
        switch(c)
        {
            case CTRL('V'): // ctrl-V
            {
            pgdn:;
                unsigned offset = CurY-WinY;
                CurY += DimY;
                if(CurY >= EditLines.size()) CurY = EditLines.size()-1;
                WinY = (CurY > offset) ? CurY-offset : 0;
                if(WinY + DimY > EditLines.size()
                && EditLines.size() > DimY) WinY = EditLines.size()-DimY;
                if(shift) dragalong = 1;
                break;
            }
            case CTRL('U'): // ctrl-U
            {
            pgup:;
                unsigned offset = CurY - WinY;
                if(CurY > DimY) CurY -= DimY; else CurY = 0;
                WinY = (CurY > offset) ? CurY-offset : 0;
                if(shift) dragalong = 1;
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
                    case 'c': case 'C': case CTRL('C'): // paste block
                    case 'y': case 'Y': case CTRL('Y'): // delete block
                    case 'd': case 'D': case CTRL('D'): // save file
                    case 'x': case 'X': case CTRL('X'): // save and exit
                        ;
                    case 'u': case 'U': case CTRL('U'): // ctrl-pgup
                        goto ctrlpgup;
                    case 'v': case 'V': case CTRL('V'): // ctrl-pgdn
                        goto ctrlpgdn;
                    case '.': // indent block
                        BlockIndent(+TabSize);
                        break;
                    case ',': // unindent block
                        BlockIndent(-TabSize);
                        break;
                    case ' ': // info about current character
                    {
                        unsigned charcode = EditLines[CurY][CurX];
                        sprintf(StatusLine,
                            "Character '%c': hex=%02X, decimal=%d, octal=%03o",
                            charcode, charcode, charcode, charcode);
                        break;
                    }
                }
                break;
            }
            case CTRL('Q'):
            {
                WaitingCtrl='';
                WaitInput();
                WaitingCtrl=0;
                break;
            }
            case CTRL('R'):
                VisGetGeometry();
                VisSetCursor();
                VisRender();
                break;
            case CTRL('G'):
                FindPair();
                if(CurY < WinY || CurY >= WinY+DimY)
                    WinY = CurY > DimY/2 ? CurY - DimY/2 : 0;
                break;
            case 0:;
            {
                switch(extch = getch())
                {
                    case 'H': // up
                        if(CurY > 0) --CurY;
                        if(CurY < WinY) WinY = CurY;
                        if(shift) dragalong = 1;
                        break;
                    case 'P': // down
                        if(CurY+1 < EditLines.size()) ++CurY;
                        if(CurY >= WinY+DimY) WinY = CurY - DimY+1;
                        if(shift) dragalong = 1;
                        break;
                    case 'K': // left
                    case 0x73: // ctrl-left
                    {
                        if(CurX > 0) --CurX;
                        else if(CurY > 0) { --CurY; goto end; }
                        if(extch == 0x73 || shift) dragalong = 1;
                        break;
                    }
                    case 'M': // right
                    case 0x74: // ctrl-right
                    {
                        ++CurX;
                        if(CurY+1 < EditLines.size()
                        && CurX >= EditLines[CurY].size())
                            { CurX = 0; ++CurY; }
                        if(extch == 0x74 || shift) dragalong = 1;
                        break;
                    }
                    case 0x47: // home
                    {
                    home:;
                        unsigned x = 0;
                        while(x < EditLines[CurY].size()
                           && EditLines[CurY][x] == ' ') ++x;
                        if(CurX == x) CurX = 0; else CurX = x;
                        if(shift) dragalong = 1;
                        break;
                    }
                    case 0x4F: // end
                    end: CurX = EditLines[CurY].size();
                        if(CurX > 0) --CurX; // past LF
                        if(shift) dragalong = 1;
                        break;
                    case 0x49: goto pgup;
                    case 0x51: goto pgdn;
                    case 0x52: // insert
                        InsertMode = !InsertMode;
                        VisSetCursor();
                        break;
                    case 0x84: // ctrl-pgup = goto beginning of file
                    ctrlpgup:
                        CurY = WinY = 0;
                        CurX = WinX = 0;
                        if(shift) dragalong = 1;
                        break;
                    case 0x76: // ctrl-pgdn = goto end of file
                    ctrlpgdn:
                        CurY = EditLines.size()-1;
                        WinY = 0;
                        if(CurY >= WinY+DimY) WinY = CurY - DimY+1;
                        goto end;
                    case 0x77: // ctrl-home = goto beginning of window (vertically)
                        CurY = WinY;
                        if(shift) dragalong = 1;
                        break;
                    case 0x75: // ctrl-end = goto end of window (vertically)
                        CurY = WinY + VidH-1;
                        if(shift) dragalong = 1;
                        break;
                    case 0x53: // delete
                        break;
                }
            }
        }
        if(dragalong)
        {
            int w=0;
            if(wasbegin || !wasend) { BlockBeginX=CurX; BlockBeginY=CurY; w=1; }
            if(!wasbegin)           { BlockEndX=CurX; BlockEndY=CurY; w=1; }
            if( !wasbegin && !wasend
            && BlockBeginX==BlockEndX
            && BlockBeginY==BlockEndY) { BlockEndX=WasX; BlockEndY=WasY; }
            if(BlockBeginY > BlockEndY
            || (BlockBeginY == BlockEndY && BlockBeginX > BlockEndX))
               {{ unsigned tmp = BlockBeginY; BlockBeginY=BlockEndY; BlockEndY=tmp; }
                { unsigned tmp = BlockBeginX; BlockBeginX=BlockEndX; BlockEndX=tmp; }}
            if(w) VisRender();
        }
    }
exit:;
    CurX = 0; CurY = WinY + VidH; InsertMode = 1;
    VisSetCursor();
    exit(0);
    return 0;
}
