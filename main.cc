#include <stdio.h>
#include <time.h>
#include <ctype.h>

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
#include "vec_sp.hh"

#include "jsf.hh"
#include "vga.hh"

const unsigned UnknownColor = 0x2400;

char StatusLine[256] =
"Ad-hoc programming editor - (C) 2011-03-08 Joel Yliluoma";

WordPtrVecType EditLines;

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

    int hadnl = 1;
    WordVecType editline;
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
                editline.resize(nextstop, UnknownColor | 0x20);
            }
            else
                editline.push_back( UnknownColor | Buf[a] );

            hadnl = 0;
            if(Buf[a] == '\n')
            {
                EditLines.push_back(editline);
                editline.clear();
                hadnl = 1;
            }
        }
    }
    if(hadnl)
    {
        EditLines.push_back(editline);
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
}
struct ApplyEngine: public JSF::Applier
{
    int finished;
    unsigned nlinestotal, nlines;
    size_t x,y, begin_line;
    ApplyEngine()
        { Reset(0); }
    void Reset(size_t line)
        { x=0; y=begin_line=line; finished=0; nlinestotal=nlines=0; }
    virtual cdecl int Get(void)
    {
        if(y >= EditLines.size() || EditLines[y].empty())
        {
            finished = 1;
            return -1;
        }
        int ret = EditLines[y][x] & 0xFF;
        if(ret == '\n')
        {
            if(kbhit()) return -1;
            ++nlines;
            if(nlines >= VidH) { nlines=0; return -1; }
            if(nlinestotal > WinY + VidH && nlines >= 4)
                { nlines=0; return -1; }
            ++nlinestotal;
        }
        ++x;
        if(x == EditLines[y].size()) { x=0; ++y; }
        //fprintf(stdout, "Gets '%c'\n", ret);
        return ret;
    }
    virtual cdecl void Recolor(register unsigned n, register unsigned attr)
    {
        attr <<= 8;
        //fprintf(stdout, "Recolors %u as %02X\n", n, attr);
        size_t px=x, py=y;
        for(; n > 0; --n)
        {
            if(px == 0) { if(!py) break; --py; px = EditLines[py].size()-1; }
            else --px;
            unsigned short&w = EditLines[py][px];
            w = (w & 0xFF) | attr;
        }
    }
};


#include "mario.hh"

unsigned CursorCounter = 0;

void VisSoftCursor(int mode)
{
    if(!C64palette) return;

    /* Modes:
     *   0 = blink ticker (100 Hz)
     *   1 = screen contents have changed
     *  -1 = request undo the cursor at current location
     */
    static unsigned short* cursor_location = 0;
    static unsigned evacuation = 0;

    unsigned char cux, cuy;
    _asm { mov ah, 3; mov bh, 0; int 0x10; mov cux, dl; mov cuy, dh; xchg cx,cx }
    unsigned short* now_location = GetVidMem(cux,cuy); //VidMem + cux + cuy * unsigned(VidW + C64palette*4);

    if(mode == 1) // screen redrawn
    {
        cursor_location = now_location;
        evacuation      = *now_location;
        CursorCounter   = 0;
    }

    if(mode == -1 || now_location != cursor_location) // undo
    {
        if(cursor_location) *cursor_location = evacuation;
        if(mode == -1) { cursor_location = 0; return; }
        cursor_location = now_location;
        evacuation      = *now_location;
        CursorCounter   = 0;
    }

    switch(CursorCounter++)
    {
        case 60:
            CursorCounter=0;
        case 0:
            *cursor_location = ((evacuation&0xF00)<<4)|evacuation;
            break;
        case 30:
            *cursor_location = evacuation;
    }
}
void VisSetCursor()
{
#ifdef __BORLANDC__
    unsigned cx = WinX > CurX ? 0 : CurX-WinX;       if(cx >= VidW) cx = VidW-1;
    unsigned cy = WinY > CurY ? 1 : CurY-WinY; ++cy; if(cy >= VidH) cy = VidH-1;
    unsigned char cux = cx, cuy = cy;
    unsigned size = InsertMode ? (VidCellHeight-2) : (VidCellHeight*2/8);
    size = (size << 8) | (VidCellHeight-1);
    if(C64palette) size = 0x3F3F;
    _asm { mov ah, 2; mov bh, 0; mov dh, cuy; mov dl, cux; int 0x10 }
    _asm { mov ah, 1; mov cx, size; int 0x10 }
    CursorCounter=0;
#endif
}
void VisRenderStatus()
{
    WordVecType Hdr(VidW);
    unsigned short* Stat = GetVidMem(0, VidH-1);

    time_t t = time(0);
    struct tm* tm = localtime(&t);

    char Buf1[80], Buf2[80];
    sprintf(Buf1, "Row %-5uCol %-5u", CurY+1,CurX+1);
    sprintf(Buf2, "%02d:%02d:%02d", tm->tm_hour,tm->tm_min,tm->tm_sec);
    unsigned x1a = VidW*12/70, x1b = x1a + strlen(Buf1);
    unsigned x2a = VidW*55/70, x2b = x2a + strlen(Buf2);

    static const unsigned char slide[] = {3,7,7,7,7,3,3,2};
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
        if(C64palette) color = 0x7020;

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
            if(C64palette) color = 0x7020;
            unsigned char c = StatusLine[p]; if(!c) c = 0x20;
            if(c != 0x20) color = (color & 0xF000u) | c;
            if(!C64palette && c != 0x20)
                switch(color>>12)
                {
                    case 8: color |= 0x700; break;
                    case 0: color |= 0x800; break;
                }
            Stat[x] = color;
            if(StatusLine[p]) ++p;
        }}
    MarioTranslate(&Hdr[0], GetVidMem(0,0), VidW);
}
void VisRender()
{
    WordVecType EmptyLine;

    unsigned winh = VidH - 1;
    if(StatusLine[0]) --winh;

    VisSoftCursor(-1);

    for(unsigned y=0; y<winh; ++y)
    {
        unsigned short* Tgt = GetVidMem(0, y+1);

        unsigned ly = WinY + y;

        WordVecType* line = &EmptyLine;
        if(ly < EditLines.size()) line = &EditLines[ly];

        unsigned lw = line->size(), lx=0, x=WinX, xl=x + VidW;
        unsigned trail = 0x0720;
        for(unsigned l=0; l<lw; ++l)
        {
            unsigned attr = (*line)[l];
            if( (attr & 0xFF) == '\n' ) break;
            ++lx;
            if(lx > x)
            {
                if( ((ly == BlockBeginY && lx-1 >= BlockBeginX)
                  || ly > BlockBeginY)
                &&  ((ly == BlockEndY && lx-1 < BlockEndX)
                  || ly < BlockEndY) )
                {
                    attr = (attr & 0xFFu)
                         | ((attr >> 4u) & 0xF00u)
                         | ((attr << 4u) & 0xF000u);
                }
                if(C64palette && islower(attr & 0xFF))
                    attr &= 0xFFDFu;

                do *Tgt++ = attr; while(lx > ++x);
                if(x >= xl) break;
            }
        }
        while(x++ < xl)
            *Tgt++ = trail;
    }
    VisSoftCursor(1);
}

unsigned wx = ~0u, wy = ~0u, cx = ~0u, cy = ~0u;

enum
{
    SyntaxChecking_IsPerfect = 0,
    SyntaxChecking_DidEdits = 1,
    SyntaxChecking_Interrupted = 2,
    SyntaxChecking_DoingFull = 3
} SyntaxCheckingNeeded = 3;

JSF::ApplyState SyntaxCheckingState;
ApplyEngine     SyntaxCheckingApplier;
void WaitInput()
{
    if(cx != CurX || cy != CurY) { cx=CurX; cy=CurY; VisSoftCursor(-1); VisSetCursor(); }
    if(StatusLine[0] && CurY >= WinY+VidH-2) WinY += 2;
    if(!kbhit())
    {
        while(CurX < WinX) WinX -= 8;
        while(CurX >= WinX + VidW) WinX += 8;
        if(wx != WinX || wy != WinY)
        {
            VisSoftCursor(-1);
            VisSetCursor();
        }
        do {
            if(SyntaxCheckingNeeded != SyntaxChecking_IsPerfect)
            {
                int horrible_sight =
                    (VidMem[VidW * 2] & 0xFF00u) == UnknownColor ||
                    (VidMem[VidW * (VidH*3/4)] & 0xFF00u) == UnknownColor;
                /* If the sight on the very screen currently
                 * is "horrible", do, as a quick fix, a scan
                 * of the current screen to at least make it
                 * look different.
                 */

                if(SyntaxCheckingNeeded != SyntaxChecking_Interrupted
                || horrible_sight)
                {
                    unsigned line = 0;

                    if( horrible_sight)
                        line = WinY;
                    else if(SyntaxCheckingNeeded != SyntaxChecking_DoingFull)
                        line = WinY>40 ? WinY-40 : 0;

                    Syntax.ApplyInit(SyntaxCheckingState);
                    SyntaxCheckingApplier.Reset(line);
                }
                Syntax.Apply(SyntaxCheckingState, SyntaxCheckingApplier);

                SyntaxCheckingNeeded =
                    !SyntaxCheckingApplier.finished
                    ? SyntaxChecking_Interrupted
                    : (SyntaxCheckingApplier.begin_line == 0)
                        ? SyntaxChecking_IsPerfect
                        : SyntaxChecking_DoingFull;

                wx=WinX; wy=WinY;
                VisRender();
            }
            if(wx != WinX || wy != WinY)
                { wx=WinX; wy=WinY; VisRender(); }
            VisRenderStatus();
            VisSoftCursor(0);
        #ifdef __BORLANDC__
            _asm { hlt }
        #endif
        } while(!kbhit());
    }
}

struct UndoEvent
{
    unsigned x, y;
    unsigned n_delete;
    WordVecType insert_chars;
};

void PerformEdit(
    unsigned x, unsigned y,
    unsigned n_delete,
    const WordVecType& insert_chars)
{
    unsigned eol_x = EditLines[y].size();
    if(eol_x > 0) --eol_x;
    if(x > eol_x) x = eol_x;

    UndoEvent event;
    event.x = x;
    event.y = y;

    // If the deletion spans across newlines, concatenate those lines first
    if(n_delete > 0)
    {
        unsigned n_lines_deleted = 0;
        while(n_delete >= EditLines[y].size() - x && y+1 < EditLines.size())
        {
            ++n_lines_deleted;
            if(BlockBeginY == y+n_lines_deleted) { BlockBeginY = y; BlockBeginX += EditLines[y].size(); }
            if(BlockEndY == y+n_lines_deleted) { BlockEndY = y; BlockEndX += EditLines[y].size(); }
            if(CurY == y+n_lines_deleted) { CurY = y; CurX += EditLines[y].size(); }
            EditLines[y].insert( EditLines[y].end(),
                EditLines[y+n_lines_deleted].begin(),
                EditLines[y+n_lines_deleted].end() );
        }
        if(n_lines_deleted > 0)
        {
            if(BlockBeginY > y) BlockBeginY -= n_lines_deleted;
            if(BlockEndY > y)   BlockEndY -= n_lines_deleted;
            if(CurY > y) CurY -= n_lines_deleted;
            EditLines.erase(EditLines.begin()+y+1,
                            EditLines.begin()+y+1+n_lines_deleted);
        }
        // Now the deletion can begin
        if(n_delete > EditLines[y].size()-x) n_delete = EditLines[y].size()-x;
        event.insert_chars.insert(
            event.insert_chars.end(),
            EditLines[y].begin() + x,
            EditLines[y].begin() + x + n_delete);
        EditLines[y].erase(
            EditLines[y].begin() + x,
            EditLines[y].begin() + x + n_delete);
        if(BlockBeginY == y && BlockBeginX > x+n_delete) BlockBeginX -= n_delete;
        else if(BlockBeginY == y && BlockBeginX > x) BlockBeginX = x;
        if(BlockEndY == y && BlockEndX > x+n_delete) BlockEndX -= n_delete;
        else if(BlockEndY == y && BlockEndX > x) BlockEndX = x;
        if(CurY == y && CurX > x+n_delete) CurX -= n_delete;
        else if(CurY == y && CurX > x) CurX = x;
    }
    // Next, check if there is something to insert
    if(!insert_chars.empty())
    {
        unsigned insert_length = insert_chars.size();
        event.n_delete = insert_length;
        
    }
    SyntaxCheckingNeeded = SyntaxChecking_DidEdits;
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
           && (EditLines[y][indent] & 0xFF) == ' ') ++indent;
        if((EditLines[y][indent] & 0xFF) == '\n') continue;
        if(indent < min_indent) min_indent = indent;
        if(indent > max_indent) max_indent = indent;
    }
    if(offset > 0)
    {
        WordVecType indentbuf(offset, UnknownColor | 0x20);
        for(unsigned y=firsty; y<=lasty; ++y)
        {
            unsigned indent = 0;
            while(indent < EditLines[y].size()
               && (EditLines[y][indent] & 0xFF) == ' ') ++indent;
            if((EditLines[y][indent] & 0xFF) == '\n') continue;
            PerformEdit(0u,y, 0u, indentbuf);
        }
        if(BlockBeginX > 0) BlockBeginX += offset;
        if(BlockEndX   > 0) BlockEndX   += offset;
    }
    else if(min_indent >= -offset)
    {
        unsigned outdent = -offset;
        WordVecType empty;
        for(unsigned y=firsty; y<=lasty; ++y)
        {
            unsigned indent = 0;
            while(indent < EditLines[y].size()
               && (EditLines[y][indent] & 0xFF) == ' ') ++indent;
            if((EditLines[y][indent] & 0xFF) == '\n') continue;
            if(indent < outdent) continue;
            PerformEdit(0u,y, outdent, empty);
        }
        if(BlockBeginX >= outdent) BlockBeginX -= outdent;
        if(BlockEndX   >= outdent) BlockEndX   -= outdent;
    }
    SyntaxCheckingNeeded = SyntaxChecking_DidEdits;
}

void GetBlock(WordVecType& block)
{
    for(unsigned y=BlockBeginY; y<=BlockEndY; ++y)
    {
        unsigned x0 = 0, x1 = EditLines[y].size();
        if(y == BlockBeginY) x0 = BlockBeginX;
        if(y == BlockEndY)   x1 = BlockEndX;
        block.insert(block.end(),
            EditLines[y].begin() + x0,
            EditLines[y].begin() + x1);
    }
}

void FindPair()
{
    int           PairDir  = 0;
    unsigned char PairChar = 0;
    unsigned char PairColor = EditLines[CurY][CurX] >> 8;
    switch(EditLines[CurY][CurX] & 0xFF)
    {
        case '{': PairChar = '}'; PairDir = 1; break;
        case '[': PairChar = ']'; PairDir = 1; break;
        case '(': PairChar = ')'; PairDir = 1; break;
        case '}': PairChar = '{'; PairDir = -1; break;
        case ']': PairChar = '['; PairDir = -1; break;
        case ')': PairChar = '('; PairDir = -1; break;
    }
    if(!PairDir) return;
    int balance = 0;
    unsigned testx = CurX, testy = CurY;
    if(PairDir > 0)
        for(;;)
        {
            if(++testx >= EditLines[testy].size())
                { testx=0; ++testy; if(testy >= EditLines.size()) return; }
            if((EditLines[testy][testx] >> 8) != PairColor) continue;
            unsigned char c = EditLines[testy][testx] & 0xFF;
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
            if((EditLines[testy][testx] >> 8) != PairColor) continue;
            unsigned char c = EditLines[testy][testx] & 0xFF;
            if(balance == 0 && c == PairChar) { CurX = testx; CurY = testy; return; }
            if(c == '{' || c == '[' || c == '(') ++balance;
            if(c == '}' || c == ']' || c == ')') --balance;
        }
}

int main()
{
#ifdef __BORLANDC__
    InstallMario();
#endif
    Syntax.Parse("c.jsf");
    FileLoad("sample.cpp");
    fprintf(stderr, "Beginning render\n");

    VgaGetMode();
    VisSetCursor();

    outportb(0x3C4, 1); int use9bit = !(inportb(0x3C5) & 1);
    outportb(0x3C4, 1); int dblw    = (inportb(0x3C5) >> 3) & 1;
    outportb(0x3D4, 9); int dblh    = inportb(0x3D5) >> 7;

    #define CTRL(c) ((c) & 0x1F)
    for(;;)
    {
        WaitInput();
        unsigned c = getch();
        int shift = 3 & (*(char*)MK_FP(0x40,0x17));
        int wasbegin = CurX==BlockBeginX && CurY==BlockBeginY;
        int wasend   = CurX==BlockEndX && CurY==BlockEndY;
        unsigned WasX = CurX, WasY = CurY;
        int dragalong=0;
        if(StatusLine[0])
        {
            StatusLine[0] = '\0';
            VisRender();
        }
        unsigned DimX = VidW, DimY = VidH-1;
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
            case CTRL('B'): goto lt_key;
            case CTRL('F'): goto rt_key;
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
                    {
                        WordVecType block, empty;
                        GetBlock(block);
                        PerformEdit(BlockBeginX,BlockBeginY, block.size(), empty);
                        // Note: ^ Assumes CurX,CurY get updated here.
                        PerformEdit(CurX,CurY, InsertMode?0u:block.size(), block);
                        break;
                    }
                    case 'c': case 'C': case CTRL('C'): // paste block
                    {
                        WordVecType block;
                        GetBlock(block);
                        BlockBeginX = BlockEndX = CurX;
                        BlockBeginY = BlockEndY = CurY;
                        PerformEdit(CurX,CurY, InsertMode?0u:block.size(), block);
                        break;
                    }
                    case 'y': case 'Y': case CTRL('Y'): // delete block
                    {
                        WordVecType block, empty;
                        GetBlock(block);
                        PerformEdit(BlockBeginX,BlockBeginY, block.size(), empty);
                        BlockEndX = BlockBeginX;
                        BlockEndY = BlockEndY;
                        break;
                    }
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
                        unsigned charcode = EditLines[CurY][CurX] & 0xFF;
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
                WaitingCtrl='Q';
                WaitInput();
                WaitingCtrl=0;
                break;
            }
            case CTRL('R'):
            refresh:
                VgaGetMode();
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
                switch(getch())
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
                    case 0x47: // home
                    {
                        #define k_home() do { \
                            unsigned x = 0; \
                            while(x < EditLines[CurY].size() \
                               && (EditLines[CurY][x] & 0xFF) == ' ') ++x; \
                            if(CurX == x) CurX = 0; else CurX = x; } while(0)
                    home:
                        k_home();
                        if(shift) dragalong = 1;
                        break;
                    }
                    case 0x4F: // end
                        #define k_end() do { \
                            CurX = EditLines[CurY].size(); \
                            if(CurX > 0) --CurX; /* past LF */ } while(0)
                    end:
                        k_end();
                        if(shift) dragalong = 1;
                        break;
                    case 'K': // left
                    {
                        #define k_left() do { \
                            if(CurX > EditLines[CurY].size()) \
                                CurX = EditLines[CurY].size()-1; \
                            else if(CurX > 0) --CurX; \
                            else if(CurY > 0) { --CurY; k_end(); } } while(0)
                    lt_key:
                        k_left();
                        if(shift) dragalong = 1;
                        break;
                    }
                    case 'M': // right
                    {
                        #define k_right() do { \
                            if(CurX+1 < EditLines[CurY].size()) \
                                ++CurX; \
                            else if(CurY+1 < EditLines.size()) \
                                { CurX = 0; ++CurY; } } while(0)
                    rt_key:
                        k_right();
                        if(shift) dragalong = 1;
                        break;
                    }
                    case 0x73: // ctrl-left (go left on word boundary)
                    {
                        k_left();
                        do k_left();
                        while( (CurX > 0 || CurY > 0)
                            && isalnum(EditLines[CurY][CurX]&0xFF));
                        k_right();
                        if(shift) dragalong = 1;
                        break;
                    }
                    case 0x74: // ctrl-right (go right on word boundary)
                    {
                        while( CurY < EditLines.size()
                            && CurX < EditLines[CurY].size()
                            && isalnum(EditLines[CurY][CurX]&0xFF) )
                            k_right();
                        do k_right();
                        while( CurY < EditLines.size()
                            && CurX < EditLines[CurY].size()
                            && !isalnum(EditLines[CurY][CurX]&0xFF) );
                        if(shift) dragalong = 1;
                        break;
                    }
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
                    delkey:
                    {
                        unsigned eol_x = EditLines[CurY].size();
                        if(eol_x > 0) --eol_x;
                        if(CurX > eol_x) { CurX = eol_x; break; } // just do end-key
                        WordVecType empty;
                        PerformEdit(CurX,CurY, 1u, empty);
                        break;
                    }
                    case 0x3B: VidH -= 1; goto newmode; // F1
                    case 0x3C: VidH += 1; goto newmode; // F2
                    case 0x3D: VidW -= 2; goto newmode; // F3
                    case 0x3E: VidW += 2; goto newmode; // F4
                    case 0x3F: use9bit = !use9bit; goto newmode; // F5
                    case 0x40: if(shift) goto shiftF6;
                        if(dblw) { dblw=0; VidW*=2; }
                        else     { dblw=1; VidW/=2; VidW&=~1; }
                        goto newmode; // F6
                    case 0x59: shiftF6:
                               dblw = !dblw; goto newmode; // shift-F6
                    case 0x41: dblh = !dblh; goto newmode; // F7
                    case 0x5A: dblh = !dblh; goto newmode; // shift-F7
                    case 0x5B: shiftF8:
                               if(VidCellHeight == 16) VidCellHeight = 8;
                          else if(VidCellHeight ==  8) VidCellHeight = 14;
                          else VidCellHeight = 16;
                          goto newmode;                    // shift-F8
                    case 0x42: // F8
                        if(shift) goto shiftF8;
                        if(VidCellHeight==16)
                            { VidCellHeight=8;
                              if(!dblh) dblh = 1;
                              if(use9bit) use9bit=0;
                            }
                        else if(VidCellHeight==8)
                            { VidCellHeight=14;
                              if(dblh) { dblh = 0; /*VidH = VidH*16/14.0+0.5;*/ }
                            }
                        else if(VidCellHeight==14)
                            { VidCellHeight=16;
                              if(!use9bit) use9bit=1;
                              /*if(!dblh) { VidH = VidH*14/16.0+0.5; }*/
                            }
                    newmode:
                        VgaSetCustomMode(VidW,VidH, VidCellHeight,
                                         use9bit, dblw, dblh);
                        VisSetCursor();
                        VisRender();
                        sprintf(StatusLine,
                            "%s: %ux%u with %ux%u font (%ux%u)",
                                VidW < 50 ? "Mode chg" : "Selected text mode",
                                VidW,VidH,
                                use9bit ? 9 : 8, VidCellHeight,
                                VidW * (use9bit ? 9 : 8) * (1+dblw),
                                VidH * VidCellHeight * (1+dblh));
                        if(C64palette)
                            strcpy(StatusLine, "READY");
                        break;
                    case 0x43: // F9
                        C64palette = !C64palette;
                        goto newmode;
                }
                break;
            }
            case CTRL('Y'): // erase line
            {
                CurX = 0;
                WordVecType empty;
                PerformEdit(CurX,CurY, EditLines[CurY].size(), empty);
                break;
            }
            case CTRL('H'): // backspace = left + delete
            {
                WordVecType empty;
                unsigned nspaces = 0;
                while(nspaces < EditLines[CurY].size()
                   && (EditLines[CurY][nspaces] & 0xFF) == ' ') ++nspaces;
                if(nspaces > 0 && CurX == nspaces)
                {
                    nspaces = 1 + (CurX-1) % TabSize;
                    CurX -= nspaces;
                    PerformEdit(CurX,CurY, nspaces, empty);
                }
                else
                {
                    if(CurX > 0) --CurX;
                    else if(CurY > 0)
                    {
                        --CurY;
                        CurX = EditLines[CurY].size();
                        if(CurX > 0) --CurX; // past LF
                    }
                    PerformEdit(CurX,CurY, 1u, empty);
                }
                break;
            }
            case CTRL('D'):
            case 0x7F:      // ctrl+backspace
                goto delkey;
            case CTRL('I'):
            {
                unsigned nspaces = TabSize - CurX % TabSize;
                WordVecType txtbuf(nspaces, 0x0720);
                PerformEdit(CurX,CurY, InsertMode?0u:nspaces, txtbuf);
                break;
            }
            case CTRL('M'): // enter
            case CTRL('J'): // ctrl+enter
            {
                unsigned nspaces = 0;
                if(InsertMode)
                {
                    // Autoindent only in insert mode
                    while(nspaces < EditLines[CurY].size()
                       && (EditLines[CurY][nspaces] & 0xFF) == ' ') ++nspaces;
                }
                WordVecType txtbuf(nspaces + 1, 0x0720);
                txtbuf[0] = 0x070A; // newline
                PerformEdit(CurX,CurY, InsertMode?0u:1u, txtbuf);
                break;
            }
            default:
            {
                WordVecType txtbuf(1, 0x0700 | c);
                PerformEdit(CurX,CurY, InsertMode?0u:1u, txtbuf);
                break;
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
#ifdef __BORLANDC__
    DeInstallMario();
#endif
    exit(0);
    return 0;
}
