#include <stdio.h>
#include <time.h>
#include <ctype.h>

#ifdef __BORLANDC__
# include <dos.h> // for MK_FP
# include <conio.h>
#else
# define cdecl
static unsigned      kbhitptr   = 0;
static unsigned char kbhitbuf[] = 
{
    0,'P', 0,'P', 0,'P',
    'K'-64, 'B'-64,
    0,'P', 0,'P',
    'K'-64, 'K'-64,
    'K'-64, 'C'-64,
    'K'-64, 'U'-64
};
# define kbhit() (kbhitptr < sizeof(kbhitbuf))
# define getch() kbhitbuf[kbhitptr++]
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

int UnsavedChanges = 0;
char* CurrentFileName = 0;

void FileLoad(char* fn)
{
    fprintf(stderr, "Loading '%s'...\n", fn);
    FILE* fp = fopen(fn, "rb");
    if(!fp) { perror(fn); return; }
    fseek(fp, 0, SEEK_END);
    rewind(fp);

    if(CurrentFileName) free(CurrentFileName);
    CurrentFileName = strdup(fn);

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
                /*while(editline.size() < nextstop)
                    editline.push_back( UnknownColor | 0x20 );*/
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
    UnsavedChanges = 0;
}
void FileNew()
{
    WinX = WinY = 0;
    CurX = CurY = 0;
    EditLines.clear();
    WordVecType emptyline;
    emptyline.push_back('\n' | UnknownColor);
    EditLines.push_back(emptyline);
    EditLines.push_back(emptyline);
    EditLines.push_back(emptyline);

    if(CurrentFileName) free(CurrentFileName);
    CurrentFileName = 0;
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

    unsigned char cux=0, cuy=0;
#ifdef __BORLANDC__
    _asm { mov ah, 3; mov bh, 0; int 0x10; mov cux, dl; mov cuy, dh; xchg cx,cx }
#endif
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
void VisPutCursorAt(unsigned cx,unsigned cy)
{
#ifdef __BORLANDC__
    if(FatMode) cx *= 2;
    unsigned char cux = cx, cuy = cy;
    unsigned size = InsertMode ? (VidCellHeight-2) : (VidCellHeight*2/8);
    size = (size << 8) | (VidCellHeight-1);
    if(C64palette) size = 0x3F3F;
    _asm { mov ah, 2; mov bh, 0; mov dh, cuy; mov dl, cux; int 0x10 }
    _asm { mov ah, 1; mov cx, size; int 0x10 }
    CursorCounter=0;
#endif
}
void VisSetCursor()
{
    unsigned cx = WinX > CurX ? 0 : CurX-WinX;       if(cx >= VidW) cx = VidW-1;
    unsigned cy = WinY > CurY ? 1 : CurY-WinY; ++cy; if(cy >= VidH) cy = VidH-1;
    VisPutCursorAt(cx,cy);
}
const char* fnpart(const char* fn)
{
    if(!fn) return "[untitled]";
    for(;;)
    {
        const char* p = strchr(fn, '/');
        if(!p) break;
        fn = p+1;
    }
    return fn;
}
void VisRenderStatus()
{
    WordVecType Hdr(VidW*2);
    unsigned short* Stat = GetVidMem(0, VidH-1);

    time_t t = time(0);
    struct tm* tm = localtime(&t);

    int showfn = VidW > 60;
    char Buf1[80], Buf2[80];
    sprintf(Buf1, "%s%sRow %-5u/%u Col %-5u",
        showfn ? fnpart(CurrentFileName) : "",
        showfn ? " " : "",
        CurY+1,
        EditLines.size(),// EditLines.capacity(),
        CurX+1);
    sprintf(Buf2, "%02d:%02d:%02d", tm->tm_hour,tm->tm_min,tm->tm_sec);
    unsigned x1a = VidW*12/70;
    unsigned x2a = VidW*55/70;
    if(showfn) x1a = 7;
    unsigned x1b = x1a + strlen(Buf1);
    unsigned x2b = x2a + strlen(Buf2);

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
        else if(x == 3 && InsertMode)
            color = (color&0xF000) | 'I';
        else if(x == 4 && UnsavedChanges)
            color = (color&0xF000) | '*';
        else if(x >= x1a && x < x1b && Buf1[x-x1a] != ' ')
            color = (color&0xF000) | Buf1[x-x1a];
        else if(x >= x2a && x < x2b && Buf2[x-x2a] != ' ')
            color = (color&0xF000) | Buf2[x-x2a];
        if(FatMode)
            { Hdr[x+x] = color; Hdr[x+x+1] = color|0x80; }
        else
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
            if(c != 0x20 || (((color >> 12)) == ((color >> 8)&0xF)))
            {
                color = (color & 0xF000u) | c;
                if((color >> 12) == ((color >> 8)&0xF)) color |= 0x700;
            }
            if(!C64palette && c != 0x20)
                switch(color>>12)
                {
                    case 8: color |= 0x700; break;
                    case 0: color |= 0x800; break;
                }
            if(FatMode)
                { Stat[x+x] = color; Stat[x+x+1] = color|0x80; }
            else
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
                if(DispUcase && islower(attr & 0xFF))
                    attr &= 0xFFDFu;

                do {
                    *Tgt++ = attr;
                    if(FatMode) *Tgt++ = attr | 0x80;
                } while(lx > ++x);
                if(x >= xl) break;
            }
        }
        while(x++ < xl)
        {
            *Tgt++ = trail;
            if(FatMode) *Tgt++ = trail | 0x80;
        }
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
} SyntaxCheckingNeeded = SyntaxChecking_DoingFull;

JSF::ApplyState SyntaxCheckingState;
ApplyEngine     SyntaxCheckingApplier;
void WaitInput(int may_redraw = 1)
{
    if(may_redraw)
    {
        if(cx != CurX || cy != CurY) { cx=CurX; cy=CurY; VisSoftCursor(-1); VisSetCursor(); }
        if(StatusLine[0] && CurY >= WinY+VidH-2) WinY += 2;
    }

    if(!kbhit())
    {
        while(CurX < WinX) WinX -= 8;
        while(CurX >= WinX + VidW) WinX += 8;
        if(may_redraw)
        {
            if(wx != WinX || wy != WinY)
            {
                VisSoftCursor(-1);
                VisSetCursor();
            }
        }
        do {
            if(may_redraw)
            {
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
            }
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
const unsigned   MaxUndo = 256;
UndoEvent UndoQueue[MaxUndo];
UndoEvent RedoQueue[MaxUndo];
unsigned  UndoHead = 0, RedoHead = 0;
unsigned  UndoTail = 0, RedoTail = 0;
char      UndoAppendOk = 0;
void AddUndo(const UndoEvent& event)
{
    unsigned UndoBufSize = (UndoHead + MaxUndo - UndoTail) % MaxUndo;
    if(UndoAppendOk && UndoBufSize > 0)
    {
        UndoEvent& prev = UndoQueue[ (UndoHead + MaxUndo-1) % MaxUndo ];
        /*if(event.n_delete == 0 && prev.n_delete == 0
        && event.x == prev.x && event.y == prev.y
          )
        {
            prev.insert_chars.insert(
                prev.insert_chars.end(),
                event.insert_chars.begin(),
                event.insert_chars.end());
            return;
        }*/
        if(event.insert_chars.empty() && prev.insert_chars.empty())
        {
            prev.n_delete += event.n_delete;
            return;
        }
    }

    if( UndoBufSize >= MaxUndo - 1) UndoTail = (UndoTail + 1) % MaxUndo;
    UndoQueue[UndoHead] = event;
    UndoHead = (UndoHead + 1) % MaxUndo;
}
void AddRedo(const UndoEvent& event)
{
    unsigned RedoBufSize = (RedoHead + MaxUndo - RedoTail) % MaxUndo;
    if( RedoBufSize >= MaxUndo - 1) RedoTail = (RedoTail + 1) % MaxUndo;
    RedoQueue[RedoHead] = event;
    RedoHead = (RedoHead + 1) % MaxUndo;
}

void PerformEdit(
    unsigned x, unsigned y,
    unsigned n_delete,
    const WordVecType& insert_chars,
    char DoingUndo = 0)
{
    unsigned eol_x = EditLines[y].size();
    if(eol_x > 0 && (EditLines[y].back() & 0xFF) == '\n') --eol_x;
    if(x > eol_x) x = eol_x;

    UndoEvent event;
    event.x = x;
    event.y = y;
    event.n_delete = 0;

    if(DoingUndo)
    {
        int s = sprintf(StatusLine,"Edit%u @%u,%u: Delete %u, insert '",
        UndoAppendOk, x,y,n_delete);
        for(unsigned b=insert_chars.size(), a=0; a<b && s<252; ++a)
        {
            char c = insert_chars[a] & 0xFF;
            if(c == '\n') { StatusLine[s++] = '\\'; StatusLine[s++] = 'n'; }
            else StatusLine[s++] = c;
        }
        sprintf(StatusLine+s, "'");
    }

    // Is there something to delete?
    if(n_delete > 0)
    {
        unsigned n_lines_deleted = 0;
        // If the deletion spans across newlines, concatenate those lines first
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

        unsigned insert_newline_count = 0;
        {for(unsigned p=0; p<insert_length; ++p)
            if( (insert_chars[p] & 0xFF) == '\n')
                ++insert_newline_count; }

        if(insert_newline_count > 0)
        {
            WordVecType nlvec(1, '\n' | UnknownColor);
            WordPtrVecType new_lines( insert_newline_count, nlvec );
            // Move the trailing part from current line to the beginning of last "new" line
            new_lines.back().assign( EditLines[y].begin() + x, EditLines[y].end() );
            // Remove the trailing part from that line
            EditLines[y].erase(  EditLines[y].begin() + x, EditLines[y].end() );
            // But keep the newline character
            EditLines[y].push_back( nlvec[0] );
            // Insert these new lines
            EditLines.insert(
                EditLines.begin()+y+1,
                new_lines.begin(),
                new_lines.end() );
            // Update cursors
            if(BlockBeginY == y && BlockBeginX >= x) { BlockBeginY += insert_newline_count; BlockBeginX -= x;  }
            else if(BlockBeginY > y) { BlockBeginY += insert_newline_count; }
            if(BlockEndY == y && BlockEndX >= x) { BlockEndY += insert_newline_count; BlockEndX -= x;  }
            else if(BlockEndY > y) { BlockEndY += insert_newline_count; }
            if(CurY == y && CurX >= x) { CurY += insert_newline_count; CurX -= x;  }
            else if(CurY > y) { CurY += insert_newline_count; }
        }
        unsigned insert_beginpos = 0;
        while(insert_beginpos < insert_length)
        {
            if( (insert_chars[insert_beginpos] & 0xFF) == '\n')
                { x = 0; ++y; ++insert_beginpos; }
            else
            {
                unsigned p = insert_beginpos;
                while(p < insert_length && (insert_chars[p] & 0xFF) != '\n')
                    ++p;

                unsigned n_inserted = p - insert_beginpos;
                EditLines[y].insert(
                    EditLines[y].begin() + x,
                    insert_chars.begin() + insert_beginpos,
                    insert_chars.begin() + p );
                if(BlockBeginY == y && BlockBeginX >= x) BlockBeginX += n_inserted;
                if(BlockEndY == y && BlockEndX >= x) BlockEndX += n_inserted;
                if(CurY == y && CurX >= x) CurX += n_inserted;
                x += n_inserted;
                insert_beginpos = p;
            }
        }
    }
    SyntaxCheckingNeeded = SyntaxChecking_DidEdits;
    switch(DoingUndo)
    {
        case 0: // normal edit
            RedoHead = RedoTail = 0; // reset redo
            AddUndo(event); // add undo
            break;
        case 1: // undo
            AddRedo(event);
            break;
        case 2: // redo
            AddUndo(event); // add undo, but don't reset redo
            break;
    }

    UnsavedChanges = 1;
}

void TryUndo()
{
    unsigned UndoBufSize = (UndoHead + MaxUndo - UndoTail) % MaxUndo;
    if(UndoBufSize > 0)
    {
        UndoHead = (UndoHead + MaxUndo-1) % MaxUndo;
        UndoEvent event = UndoQueue[UndoHead]; // make copy
        PerformEdit(event.x, event.y, event.n_delete, event.insert_chars, 1);
    }
}
void TryRedo()
{
    unsigned RedoBufSize = (RedoHead + MaxUndo - RedoTail) % MaxUndo;
    if(RedoBufSize > 0)
    {
        RedoHead = (RedoHead + MaxUndo-1) % MaxUndo;
        UndoEvent event = RedoQueue[RedoHead]; // make copy
        PerformEdit(event.x, event.y, event.n_delete, event.insert_chars, 2);
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

int use9bit, dblw, dblh;

int SelectFont()
{
    struct opt
    {
        unsigned char px, py;
        unsigned w[5], h[5], cx[5],cy[5],wid[5];
    };
    opt options[] =
    {
        { 8, 8 }, { 9,  8},
        { 8,14 },
        { 8,16 }, { 9, 16},
        { 8,19 }, { 9, 19},
        { 8,32 }, { 9, 32},
        {16,32 }
    };
    if(VidCellHeight == 8 || VidCellHeight == 14)
    {
        // Put 16-pixel modes to the beginning of list for quick swapping
        opt tmp;
        tmp = options[0]; options[0] = options[4]; options[4] = tmp;
        tmp = options[1]; options[1] = options[3]; options[3] = tmp;
    }

    unsigned curw = VidW * (/*use9bit ? 9 :*/ 8) * (FatMode?2:1);// * (1+dblw);
    unsigned curh = VidH * VidCellHeight                        ;// * (1+dblh);

    const unsigned noptions = sizeof(options) / sizeof(*options);
    unsigned char wdblset[5] = { dblw, dblw, !dblw, dblw, !dblw };
    unsigned char hdblset[5] = { dblh, dblh, dblh, !dblh, !dblh };
    signed int maxwidth[5] = { 0,0,0,0,0 };
    for(unsigned n=0; n<noptions; ++n)
    {
        opt& o = options[n];
        o.w[0] = VidW;
        o.h[0] = VidH;
        {for(unsigned m=1; m<5; ++m)
        {
            unsigned sx = curw, sy = curh;
            if(dblw && !wdblset[m]) sx *= 2;
            if(dblh && !hdblset[m]) sy *= 2;
            if(!dblw && wdblset[m]) sx /= 2;
            if(!dblh && hdblset[m]) sy /= 2;
            o.w[m] = sx / (o.px & ~7u);
            o.h[m] = sy / o.py;
            if(VidH != o.h[m])
                { if(o.h[m] == 24) o.h[m] = 25;
                  if(o.h[m] == 42) o.h[m] = 43;
                  if(o.h[m] == 48) o.h[m] = 50;
                  if(o.h[m] == 28) o.h[m] = 30;
                }
            o.w[m] &= ~1;
        }}
        for(unsigned m=0; m<5; ++m)
        {
            int wid = 6; // ".40x25" = 6 chars
            if(o.w[m] >= 100) wid += 1;
            if(o.h[m] >= 100) wid += 1;
            if(wid > maxwidth[m]) maxwidth[m] = wid;
        }
    }
    unsigned cancelx = 28, cancely = VidH-1;

    /* Resolution change options:
     *   Keep current
     *   Preserve pixel count
     *   Flip hdouble & preserve pixel count
     *   Flip vdouble & preserve pixel count
     *   Flip both    & preserve pixel count
     *
     * 16x32 40x25 40x25 40x25 80x25 80x25 80x25
     */

    int sel_x = 0, sel_y = -1;
    {for(unsigned y=0; y<noptions; ++y)
        if(options[y].px == (use9bit?9:8)*(FatMode?2:1)
        && options[y].py == VidCellHeight)
            { sel_y = y; break; }}
    for(;;)
    {
        VisSoftCursor(-1);
        sprintf(StatusLine, "Please select new font size [Cancel]");
        VisRenderStatus();
        StatusLine[0] = 0;

        char marker_empty  = '.';
        {for(unsigned y=0; y<noptions; ++y)
        {
            unsigned yy = VidH-noptions+y-1;
            unsigned short* p = GetVidMem(0,yy);
            opt& o = options[y];

            char Buf[80], Buf1[16], Bufs[5][16];
            sprintf(Buf1, "%ux%u", o.px, o.py);
            {for(unsigned m=0; m<5; ++m)
                o.wid[m] = sprintf(Bufs[m], "%c%ux%u",
                    marker_empty, o.w[m], o.h[m])-1; }
            {char*s = Buf+sprintf(Buf,"%5s:", Buf1);
            for(unsigned m=0; m<5; ++m)
                s += sprintf(s,"%*s", -maxwidth[m], Bufs[m]);}
            {for(unsigned q=0,m=0,a=0; a<VidW; ++a)
                { unsigned short c = Buf[q];
                  if(c == marker_empty)
                  {
                      c = ' ';
                      o.cy[m]   = yy;
                      o.cx[m++] = a+1;
                  }
                  if(c) ++q; else c = ' ';
                  c |= 0x7000;
                  if(FatMode) { p[2*a]=c; p[2*a+1]=c|0x80; } else p[a] = c;
        }   }   }}


        unsigned x=cancelx, xw=8, y=cancely;
        if(sel_y >= 0)
            { x  = options[sel_y].cx[sel_x];
              y  = options[sel_y].cy[sel_x];
              xw = options[sel_y].wid[sel_x];
            }
        VisPutCursorAt(x-1,y);
        if(FatMode) xw *= 2;
        unsigned short* p = GetVidMem(x,y);
        for(unsigned m=0; m<xw; ++m)
        {
            unsigned short c = p[m];
            p[m] = (c&0xFF) | ((c>>4)&0xF00) | ((c&0xF00)<<4);
        }
    rewait:
        WaitInput(0);
        unsigned c = getch();
        if(c == 27 || c == 3)
            { gotesc:
              if(sel_y == -1) break;
              sel_y = -1; }
        else if(c == 13 || c == 10)
            break;
        else if(c == 'A'-64) goto hom;
        else if(c == 'E'-64) goto end;
        else if(c == 'U'-64) goto pgup;
        else if(c == 'V'-64) goto pgdn;
        else if(c == '-')
            { if(sel_y) sel_y=0; else sel_x=0; }
        else if(c == ' ' || c == '+')
            { if(sel_y < noptions-1) sel_y=noptions-1; else sel_x=4; }
        else if(c == 0)
            switch(getch())
            {
                case 'H': up:
                    if(sel_y < 0) sel_y = noptions;
                    --sel_y;
                    break;
                case 'P': dn:
                    if(++sel_y >= noptions) sel_y = -1;
                    break;
                case 'K': if(sel_x>0) --sel_x; else { sel_x=4; goto up; } break;
                case 'M': if(sel_x<4) ++sel_x; else { sel_x=0; goto dn; } break;
                case 0x47: hom: if(sel_x > 0) sel_x=0; else sel_y=0; break;
                case 0x4F: end: if(sel_x < 4) sel_x=4; else sel_y=noptions-1; break;
                case 0x49: pgup: sel_y = 0; break;
                case 0x51: pgdn: sel_y = noptions-1; break;
                case 0x42: goto gotesc; // F8
                default: goto rewait;
            }
        else goto rewait;
    }
    if(sel_y == -1) return 0;
    dblw = wdblset[sel_x];
    dblh = hdblset[sel_x];
    VidCellHeight = options[sel_y].py;
    VidW          = options[sel_y].w[sel_x];
    VidH          = options[sel_y].h[sel_x];
    use9bit = (options[sel_y].px == 9) || (options[sel_y].px == 18);
    FatMode = options[sel_y].px >= 16;
    return 1;
}

int VerifyUnsavedExit(const char* action)
{
    if(!UnsavedChanges) return 1;
    VisSoftCursor(-1);
    int s = sprintf(StatusLine, "FILE IS UNSAVED. PROCEED WITH %s? Y/N  ", action);
    VisPutCursorAt(s-1, VidH-1);
    VisRenderStatus();
    int decision = 0;
    for(;;)
    {
        WaitInput(1);
        int c = getch();
        if(c == 'Y' || c == 'y') { decision=1; break; }
        if(c == 'C'-64
        || c == 'N' || c == 'n') { decision=0; break; }
        if(c == 0) getch();
    }
    StatusLine[0] = 0;
    VisSetCursor();
    VisRender();
    return decision;
}
int PromptText(const char* message, const char* deftext, char** result)
{
    const int arrow_left = 0x11, arrow_right = 0x10;
    char data[256] = { 0 };
    if(deftext) strcpy(data, deftext);
    unsigned begin_offset = sprintf(StatusLine, "%s", message);
    char* buffer = StatusLine+begin_offset;
    unsigned firstPos = 0, curPos = strlen(data);
    unsigned maxLen = sizeof(StatusLine)-begin_offset-1;
    unsigned sizex  = VidW - begin_offset;
    for(;;)
    {
        unsigned data_length = strlen(data);
        if(curPos > data_length) curPos = data_length;
        if(firstPos > curPos) firstPos = curPos;
        if(firstPos + (sizex - 2) < curPos) firstPos = curPos - (sizex - 2);
        // Draw
        memset(buffer, ' ', sizex);
        strcpy(buffer+1, data+firstPos);
        buffer[0]       = firstPos > 0 ? arrow_left : ' ';
        buffer[sizex-1] = data_length-firstPos > (sizex-2) ? arrow_right : ' ';
        VisRenderStatus();
        VisPutCursorAt(begin_offset + (curPos-firstPos+1), VidH-1);

        WaitInput();
        int c = getch();
        switch(c)
        {
            case 'C'-64:
                StatusLine[0] = '\0';
                return 0;
            case 'B'-64: goto kb_lt;
            case 'F'-64: goto kb_rt;
            case 'A'-64: goto kb_hom;
            case 'E'-64: goto kb_end;
            case 0:
                switch(getch())
                {
                    case 'K': kb_lt: if(curPos>0) --curPos; break;
                    case 'M': kb_rt: if(curPos<data_length) curPos++; break;
                    case 0x47: kb_hom: curPos = 0; break;
                    case 0x4F: kb_end: curPos = data_length; break;
                    case 0x53: kb_del: if(curPos >= data_length) break;
                                       strcpy(data+curPos, data+curPos+1);
                                       break;
                    case 0x52: InsertMode = !InsertMode; break;
                }
                break;
            case 'H'-64:
                if(curPos <= 0) break;
                strcpy(data+curPos-1, data+curPos);
                --curPos;
                if(firstPos > 0) --firstPos;
                break;
            case 'D'-64: goto kb_del;
            case 'Y'-64: curPos = 0; data[0] = '\0'; break;
            case '\n': case '\r':
            {
                StatusLine[0] = '\0';
                if(*result) free(*result);
                *result = strdup(data);
                return 1;
            }
            default:
                if(!InsertMode && curPos >= data_length)
                    strcpy(data+curPos, data+curPos+1);
                if(strlen(data) < maxLen)
                {
                    if(firstPos > curPos) firstPos = curPos;
                    memmove(data+curPos+1, data+curPos, strlen(data+curPos)+1);
                    data[curPos++] = c;
                }
        }
    }
}
void InvokeSave(int ask_name)
{
    if(ask_name || !CurrentFileName)
    {
        char* name = 0;
        int decision = PromptText("Save to:",
            CurrentFileName ? CurrentFileName : "",
            &name);
        VisSetCursor();
        VisRender();
        if(!decision || !name || !*name)
        {
            if(name) free(name);
            return;
        }
        if(CurrentFileName) free(CurrentFileName);
        CurrentFileName = name;
    }
    FILE* fp = fopen(CurrentFileName, "wb");
    if(!fp)
    {
        perror(CurrentFileName);
        return;
    }
    for(unsigned a=0; a<EditLines.size(); ++a)
    {
        for(unsigned b=EditLines[a].size(), x=0; x<b; ++x)
        {
            char c = EditLines[a][x] & 0xFF;
            if(c == '\n') fputc('\r', fp);
            fputc(c, fp);
        }
    }
    unsigned long size = ftell(fp);
    fclose(fp);
    sprintf(StatusLine, "Saved %lu bytes to %s", size, CurrentFileName);
    VisRenderStatus();
    UnsavedChanges = 0;
}
void InvokeLoad()
{
    char* name = 0;
    int decision = PromptText("Load what:",
        CurrentFileName ? CurrentFileName : "",
        &name);
    VisSetCursor();
    VisRender();
    if(!decision || !name || !*name)
    {
        if(name) free(name);
        return;
    }
    FileLoad(name);
    free(name);

    SyntaxCheckingNeeded = SyntaxChecking_DoingFull;
    UndoHead=UndoTail=0;
    RedoHead=RedoTail=0;
    UndoAppendOk=0;
}

int main(int argc, char**argv)
{
#ifdef __BORLANDC__
    InstallMario();
#endif
    Syntax.Parse("c.jsf");
    FileNew();
    if(argc == 2)
    {
        FileLoad(argv[1]);
    }

    fprintf(stderr, "Beginning render\n");

    VgaGetMode();
    VisSetCursor();

#ifdef __BORLANDC__
    outportb(0x3C4, 1); use9bit = !(inportb(0x3C5) & 1);
    outportb(0x3C4, 1); dblw    = (inportb(0x3C5) >> 3) & 1;
    outportb(0x3D4, 9); dblh    = inportb(0x3D5) >> 7;
#endif

    #define CTRL(c) ((c) & 0x1F)
    for(;;)
    {
        WaitInput();
        unsigned c = getch();
#ifdef __BORLANDC__
        int shift = 3 & (*(char*)MK_FP(0x40,0x17));
#else
        int shift = 0;
#endif
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
        char WasAppend = 0;
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
            try_exit:
                /* TODO: Check if unsaved */
                if(VerifyUnsavedExit("EXIT"))
                    goto exit;
                break;
            }
            case 0x7F:      // ctrl+backspace
            {
                TryUndo();
                break;
            }
            case CTRL('W'): if(WinY > 0) --WinY; break;
            case CTRL('Z'): if(WinY+DimY < EditLines.size()) ++WinY; break;
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
                        BlockEndX = CurX; BlockEndY = CurY;
                        unsigned x = CurX, y = CurY;
                        PerformEdit(CurX,CurY, InsertMode?0u:block.size(), block);
                        BlockBeginX = x; BlockBeginY = y;
                        break;
                    }
                    case 'c': case 'C': case CTRL('C'): // paste block
                    {
                        WordVecType block;
                        GetBlock(block);
                        BlockEndX = CurX; BlockEndY = CurY;
                        unsigned x = CurX, y = CurY;
                        PerformEdit(CurX,CurY, InsertMode?0u:block.size(), block);
                        BlockBeginX = x; BlockBeginY = y;
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
                    {
                        InvokeSave( 1 );
                        break;
                    }
                    case 'x': case 'X': case CTRL('X'): // save and exit
                    {
                        InvokeSave( 0 );
                        goto try_exit;
                    }
                    case 'e': case 'E': case CTRL('E'): // load new file
                    case 'o': case 'O': case CTRL('O'):
                    {
                        if(VerifyUnsavedExit(VidW < 44 ? "FOPEN" : "FILE OPEN"))
                            InvokeLoad();
                        break;
                    }
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
                            VidW >= 60
                            ? "Character '%c': hex=%02X, decimal=%d, octal=%03o"
                            : "Character '%c': 0x%02X = %d = '\\0%03o'",
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
            case CTRL('T'): goto askmode;
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
                            if(CurX > 0 && (EditLines[CurY].back() & 0xFF) == '\n') \
                                --CurX; /* past LF */ } while(0)
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
                        if(eol_x > 0 && (EditLines[CurY].back() & 0xFF) == '\n') --eol_x;
                        if(CurX > eol_x) { CurX = eol_x; break; } // just do end-key
                        WordVecType empty;
                        PerformEdit(CurX,CurY, 1u, empty);
                        WasAppend = 1;
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
                    case 0x5B: // shift-F8
                    case 0x42: // F8
                    askmode:
                        if(SelectFont())
                        {
                        newmode:
                            if(VidW > 240) VidW = 240;
                            if(VidH > 127) VidH = 127;
                            VgaSetCustomMode(VidW,VidH, VidCellHeight,
                                             use9bit, dblw, dblh);
                            sprintf(StatusLine,
                                "%s: %ux%u with %ux%u font (%ux%u)",
                                    VidW < 53 ? "Mode chg" : "Selected text mode",
                                    VidW,VidH,
                                    (use9bit ? 9 : 8) * (FatMode?2:1),
                                    VidCellHeight,
                                    VidW * (use9bit ? 9 : 8) * (1+dblw) * (FatMode?2:1),
                                    VidH * VidCellHeight * (1+dblh));
                        }
                        VisSetCursor();
                        VisRender();
                        if(C64palette)
                        {
                            char res[8];
                            sprintf(res, "%ux%u", VidW,VidH);
                            sprintf(StatusLine, "READY%*s", VidW-5, res);
                        }
                        break;
                    case 0x43: // F9
                        C64palette = !C64palette;
                        DispUcase = C64palette;
                        if(C64palette) use9bit = 0;
                        goto newmode;
                    case 0x44: // F10
                        DispUcase = !DispUcase;
                        if(C64palette)
                        {
                            char res[8];
                            sprintf(res, "%ux%u", VidW,VidH);
                            sprintf(StatusLine, "READY%*s", VidW-5, res);
                        }
                        VisRender();
                        break;
                    case 0x2C: TryUndo(); break; // alt+Z
                    case 0x15: TryRedo(); break; // alt+Y
                    case 0x13: TryRedo(); break; // alt+R
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
                WasAppend = 1;
                break;
            }
            case CTRL('D'):
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
                WasAppend = 1;
                break;
            }
            default:
            {
                WordVecType txtbuf(1, 0x0700 | c);
                PerformEdit(CurX,CurY, InsertMode?0u:1u, txtbuf);
                WasAppend = 1;
                break;
            }
        }
        UndoAppendOk = WasAppend;
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
    if(FatMode || C64palette)
    {
        FatMode=0;
        C64palette=0;
        VgaSetCustomMode(VidW,VidH, VidCellHeight,
                         use9bit, dblw, dblh);
    }
    VisSetCursor();
#ifdef __BORLANDC__
    DeInstallMario();
#endif
    exit(0);
    return 0;
}
