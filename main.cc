/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <process.h> // For Cycles adjust on DOSBOX

#define CTRL(c) ((c) & 0x1F)

static const int ENABLE_DRAG = 0;
static int use9bit, dblw, dblh;
#define columns 1
volatile unsigned long MarioTimer = 0;

// Return just the filename part of pathfilename
const char* fnpart(const char* fn)
{
    if(!fn) return "Untitled";
    for(;;)
    {
        const char* p = strchr(fn, '/');
        if(!p) {
            p = strchr(fn, '\\');
            if(!p) break;
        }
        fn = p+1;
    }
    return fn;
}

#ifdef __BORLANDC__
# include <dos.h> // for MK_FP
# include <conio.h>

static unsigned char KbStatus = 0;
static int MyKbhit()
{
    if(!KbStatus)
    {
        _asm { mov ah, 0x01; int 0x16; jnz keyfound }
        return 0;
    }
keyfound:
    return 1;
}
static int MyGetch()
{
    register unsigned char r = KbStatus;
    if(r) { KbStatus=0; return r; }
    _asm { xor ax,ax; int 0x16 }
    if(_AL) return _AL;
    KbStatus    = _AH;
    return 0;
}
#define kbhit MyKbhit
#define getch MyGetch

#else
# define cdecl
static unsigned      kbhitptr   = 0;
static unsigned char kbhitbuf[] =
{
    // simulated input for testing with valgrind
    CTRL('K'),'L', '1','0','0','0','\n',
    CTRL('K'),'k',
    CTRL('K'),'u',
    CTRL('K'),'b',
    CTRL('K'),'y'
};
# define kbhit() (kbhitptr < sizeof(kbhitbuf) && rand()%100 == 0)
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

struct Anchor
{
    size_t x, y;
    Anchor() : x(0), y(0) { }
};

JSF     Syntax;

enum { MaxSavedCursors = 4, NumCursors = MaxSavedCursors + 2 };

Anchor Win, SavedCursors[NumCursors];

int     InsertMode=1, WaitingCtrl=0, CurrentCursor=0;
unsigned TabSize  =4;

#define Cur        SavedCursors[CurrentCursor]
#define BlockBegin SavedCursors[MaxSavedCursors  ]
#define BlockEnd   SavedCursors[MaxSavedCursors+1]

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
    int got_cr = 0;
    for(;;)
    {
        unsigned char Buf[512];
        size_t r = fread(Buf, 1, sizeof(Buf), fp);
        if(r == 0) break;
        for(size_t a=0; a<r; ++a)
        {
            if(Buf[a] == '\r' && !got_cr) { got_cr = 1; continue; }
            int maxrepeat = (got_cr && Buf[a] != '\n') ? 2 : 1;
            for(int repeat=0; repeat<maxrepeat; ++repeat)
            {
                unsigned char c = Buf[a];
                if(repeat == 0 && got_cr) c = '\n';

                if(c == '\t')
                {
                    size_t nextstop = editline.size() + TabSize;
                    nextstop -= nextstop % TabSize;
                    editline.resize(nextstop, UnknownColor | 0x20);
                    /*while(editline.size() < nextstop)
                        editline.push_back( UnknownColor | 0x20 );*/
                }
                else
                    editline.push_back( UnknownColor | c );

                hadnl = 0;
                if(c == '\n')
                {
                    EditLines.push_back(editline);
                    editline.clear();
                    hadnl = 1;
                }
            }
            got_cr = Buf[a] == '\r';
        }
    }
    if(hadnl)
    {
        EditLines.push_back(editline);
        editline.clear();
    }
    fclose(fp);
    Win = Cur = Anchor();
    UnsavedChanges = 0;
}
void FileNew()
{
    Win = Cur = Anchor();
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
    unsigned pending_recolor, pending_attr;
    ApplyEngine()
        { Reset(0); }
    void Reset(size_t line)
        { x=0; y=begin_line=line; finished=0; nlinestotal=nlines=0;
          pending_recolor=0;
          pending_attr   =0;
        }
    virtual cdecl int Get(void)
    {
        FlushColor();
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
            if(nlinestotal > Win.y + VidH && nlines >= 4)
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
        if(n < pending_recolor) FlushColor();
        pending_recolor = n;
        pending_attr    = attr << 8;
    }
private:
    void FlushColor()
    {
        register unsigned n    = pending_recolor;
        register unsigned attr = pending_attr;
        //fprintf(stdout, "Recolors %u as %02X\n", n, attr);
        size_t px=x, py=y;
        for(; n > 0; --n)
        {
            if(px == 0) { if(!py) break; --py; px = EditLines[py].size()-1; }
            else --px;
            unsigned short&w = EditLines[py][px];
            w = (w & 0xFF) | attr;
        }
        pending_recolor = 0;
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
    if(columns > 1)
    {
        register unsigned short h = (VidH-1) / columns;
        cx += ((cy-1) / h) * VidW;
        cy =  ((cy-1) % h) + 1;
    }
    if(FatMode) cx *= 2;
    unsigned char cux = cx, cuy = cy;

    unsigned size = InsertMode ? (VidCellHeight-2) : (VidCellHeight*2/8);
    size = (size << 8) | (VidCellHeight-1);
    if(C64palette) size = 0x3F3F;
    _asm { mov ah, 2; mov bh, 0; mov dh, cuy; mov dl, cux; int 0x10 }
    _asm { mov ah, 1; mov cx, size; int 0x10 }
    CursorCounter=0;
#else
    cx=cx; cy=cy;
#endif
}
void VisSetCursor()
{
    unsigned cx = Win.x > Cur.x ? 0 : Cur.x-Win.x;       if(cx >= VidW) cx = VidW-1;
    unsigned cy = Win.y > Cur.y ? 1 : Cur.y-Win.y; ++cy; if(cy >= VidH) cy = VidH-1;
    VisPutCursorAt(cx+1,cy+1);
}

static long CYCLES_Current = 80000l;
static long CYCLES_Goal    = 80000l;
static int Cycles_Trend = 0;

static void Cycles_Adjust(int direction)
{
    Cycles_Trend = direction;
}
static void Cycles_Check()
{
    // For Cycles adjust on DOSBOX
    if(Cycles_Trend > 0 && CYCLES_Goal < 1000000l)
    {
        CYCLES_Goal = CYCLES_Goal * 125l / 100l;
    }
    if(Cycles_Trend < 0 && CYCLES_Goal > 12000l)
    {
        if(CYCLES_Goal > 150000l)
            CYCLES_Goal = CYCLES_Goal * 5l / 10l;
        else
            CYCLES_Goal = CYCLES_Goal * 7l / 10l;
    }

    if(CYCLES_Goal/3000l != CYCLES_Current/3000l)
    {
        //static unsigned counter=0;
        //if(counter != 1) return;//if(++counter < 5) return;
        //counter=0;
        char Buf[64];
        sprintf(Buf, " CYCLES=%ld", CYCLES_Current = CYCLES_Goal);

        unsigned psp = getpsp();
        sprintf( (char*) MK_FP(psp, 0x80), "%s", Buf);

         _asm { db 0xFE,0x38,0x06,0x00 }
    }
    Cycles_Trend = 0;
}

void VisRenderStatus()
{
    unsigned StatusWidth = VidW*columns;

    WordVecType Hdr(StatusWidth*2);
    unsigned short* Stat = GetVidMem(0, (VidH-1) / columns, 1);

    time_t t = time(0);
    struct tm* tm = localtime(&t);
    /*
    static unsigned long begintime = *(unsigned long*)MK_FP(0x40,0x6C);
    unsigned long nowtime          = *(unsigned long*)MK_FP(0x40,0x6C);
    unsigned long time = (47*60+11) - 759 + (nowtime-begintime)*(65536.0/0x1234DC);
    tm->tm_hour = 18;
    tm->tm_min  = time/60;
    tm->tm_sec  = time%60;
    */
    char Buf1[80], Buf2[32], Buf3[16], Buf4[80], Buf5[16];
    unsigned StatusLength = strlen(StatusLine);

    sprintf(Buf2, "%5u lines | %02d:%02d:%02d",
        (unsigned) EditLines.size(),
        tm->tm_hour,tm->tm_min,tm->tm_sec);
    sprintf(Buf3, "%c    %05d:%03d",
        InsertMode ? 0xB3 : 'O',
        (unsigned)(Cur.y+1), (unsigned)(Cur.x+1));
                  //I*^I
    unsigned x3a = VidW-15;
    unsigned x2a = x3a - 9 - 14;
    unsigned x1a = 0;
    unsigned x4a = 2    ;
    unsigned x5a = VidW-6;

    int status_where = -1;
    if(StatusLength > 0)
    {
        if(StatusLength < x3a)
            status_where = 0; // Bottom
        else
            status_where = 1; // Top
    }

    if(status_where == 1)
        { strncpy(Buf4, StatusLine, 80); x5a = VidW; }
    else
    {
        sprintf(Buf4, "File  Edit  View  Search  Run  Bogus  Menu  Bar");
        sprintf(Buf5, "Help");
    }

    if(status_where == 0)
    {
        strncpy(Buf1, StatusLine, 80);
        if(StatusLength >= x2a) x2a = x3a;
    }
    else
        sprintf(Buf1, " QBasic masquerade mode engaged.");

    {for(unsigned x=0; x<VidW; ++x)
    {
        unsigned color = 0x3020;
        // bottom bar
        if(x >= x3a)
        {
            /**/ if(x==x3a+1 && WaitingCtrl)
                color = (color&0xF000) | '^';
            else if(x==x3a+2 && WaitingCtrl)
                color = (color&0xF000) | WaitingCtrl;
            else if(x==x3a+4 && UnsavedChanges)
                color = (color&0xF000) | '*';
            else
                color = (color&0xF000) | (unsigned char)Buf3[x-x3a];
        }
        else if(x >= x2a) { if(Buf2[x-x2a])
            color = 0x3000 | Buf2[x-x2a]; else ++x2a; }
        else if(x >= x1a) { if(Buf1[x-x1a])
            color = (status_where==0 ? 0x3F00 : 0x3000)
                     | Buf1[x-x1a]; else ++x1a; }
        if(FatMode)
            { Stat[x+x] = color; Stat[x+x+1] = color|0x80; }
        else
            Stat[x] = color;

        // top bar
        color = 0x7020;
        if(x >= x5a) { if(Buf5[x-x5a])
            color = 0x7000 + Buf5[x-x5a]; else ++x5a; }
        else if(x >= x4a) { if(Buf4[x-x4a])
            color = 0x7000 + Buf4[x-x4a]; else ++x4a; }

        if(FatMode)
            { Hdr[x+x] = color; Hdr[x+x+1] = color|0x80; }
        else
            Hdr[x] = color;
    }}

    //MarioTranslate(&Hdr[0], GetVidMem(0,VidH-1), VidW);
    MarioTranslate(&Hdr[0], GetVidMem(0,0,1), StatusWidth);

#ifdef __BORLANDC__
    if(0 && !kbhit())
    {
        /* Wait retrace */
        /**/ _asm { mov dx, 0x3DA }
        WR1: _asm { in al, dx; and al, 8; jnz WR1 }
        WR2: _asm { in al, dx; and al, 8; jz WR2 }
    }
#endif
}
void VisRender()
{
    WordVecType EmptyLine;

    unsigned winh = VidH - 3;

    VisSoftCursor(-1);

    {unsigned short* Tgt = GetVidMem(0, 1);
    const char* editfn = fnpart(CurrentFileName);
    unsigned editfnlen = strlen(editfn);
    unsigned editfnpos = (VidW - (editfnlen+2)) / 2;
    *Tgt++ = 0x17DA; // corner
    for(unsigned x=1; x<VidW-1; ++x)
    {
        if(x >= editfnpos)
        {
            unsigned p = x - editfnpos;
            if(p >= 1 && p <= editfnlen)
                { *Tgt++ = 0x7100 + editfn[p-1]; continue; }
            if(p < editfnlen+2)
                { *Tgt++ = 0x7120; continue; }
        }
        /**/ if(x == VidW-5) *Tgt++ = 0x17B4; //
        else if(x == VidW-4) *Tgt++ = 0x7112; //
        else if(x == VidW-3) *Tgt++ = 0x17C3; //
        else *Tgt++ = 0x17C4; // horizontal line
    }
    *Tgt++ = 0x17BF; // corner
    }

    // How much does the window show of the entire program?
    unsigned move = EditLines.size(); if(move > winh) move -= winh;
    unsigned scrollbar_height = (winh-2) * winh / EditLines.size();
    if(scrollbar_height < 1) scrollbar_height = 1;
    unsigned scrollbar_begin = 1 + (winh-2-scrollbar_height) * Win.y / move;
    unsigned scrollbar_end   = scrollbar_begin + scrollbar_height;

    for(unsigned y=0; y<winh; ++y)
    {
        unsigned short* Tgt = GetVidMem(0, y+2);

        unsigned ly = Win.y + y;

        WordVecType* line = &EmptyLine;
        if(ly < EditLines.size()) line = &EditLines[ly];

        unsigned lw = line->size(), lx=0, x=Win.x, xl=x + VidW - 2;
        unsigned trail = 0x1720, verticalbar = 0x17B3;

        *Tgt++ = verticalbar;

        for(unsigned l=0; l<lw; ++l)
        {
            unsigned attr = (*line)[l];
            if( (attr & 0xFF) == '\n' ) break;
            ++lx;
            if(lx > x)
            {
                if( ((ly == BlockBegin.y && lx-1 >= BlockBegin.x)
                  || ly > BlockBegin.y)
                &&  ((ly == BlockEnd.y && lx-1 < BlockEnd.x)
                  || ly < BlockEnd.y) )
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
        if(y == 0)
            *Tgt++ = 0x7018; // arrow up
        else if(y == winh-1)
            *Tgt++ = 0x7019; // arrow down
        else if(y >= scrollbar_begin
             && y < scrollbar_end)
            *Tgt++ = 0x0020; // scrollbar 
        else
            *Tgt++ = 0x70B0; // scrollbar background

        *Tgt++ = verticalbar;
    }
    VisSoftCursor(1);

#ifdef __BORLANDC__
    if(0 && !kbhit())
    {
        /* Wait retrace */
        /**/ _asm { mov dx, 0x3DA }
        WR1: _asm { in al, dx; and al, 8; jnz WR1 }
        WR2: _asm { in al, dx; and al, 8; jz WR2 }
    }
#endif
}

unsigned wx = ~0u, wy = ~0u, cx = ~0u, cy = ~0u;

enum SyntaxCheckingType
{
    SyntaxChecking_IsPerfect = 0,
    SyntaxChecking_DidEdits = 1,
    SyntaxChecking_Interrupted = 2,
    SyntaxChecking_DoingFull = 3
} SyntaxCheckingNeeded = SyntaxChecking_DoingFull;

JSF::ApplyState SyntaxCheckingState;
ApplyEngine     SyntaxCheckingApplier;

/* TODO: In syntax checking: If the syntax checker ever reaches the current editing line,
 *                           make a save in the beginning of the line and use that for resuming
 *                           instead of backtracking to the context
 */

// How many lines to backtrack
#define SyntaxChecking_ContextOffset 50

void WaitInput(int may_redraw = 1)
{
    if(may_redraw)
    {
        if(cx != Cur.x || cy != Cur.y) { cx=Cur.x; cy=Cur.y; VisSoftCursor(-1); VisSetCursor(); }
        while(Cur.y >= Win.y+VidH-3) Win.y += 1;
    }

    Cycles_Adjust(1);

    if(!kbhit())
    {
        while(Cur.x < Win.x) Win.x -= 8;
        while(Cur.x >= Win.x + VidW-2) Win.x += 8;
        if(may_redraw)
        {
            if(wx != Win.x || wy != Win.y)
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
                            line = Win.y;
                        else if(SyntaxCheckingNeeded != SyntaxChecking_DoingFull)
                            line = Win.y>SyntaxChecking_ContextOffset ? Win.y-SyntaxChecking_ContextOffset : 0;

                        Syntax.ApplyInit(SyntaxCheckingState);
                        SyntaxCheckingApplier.Reset(line);
                    }
                    Syntax.Apply(SyntaxCheckingState, SyntaxCheckingApplier);

                    if(SyntaxCheckingNeeded == SyntaxChecking_Interrupted)
                    {
                        // If the syntax checking was interrupted
                    }

                    SyntaxCheckingNeeded =
                        !SyntaxCheckingApplier.finished
                        ? SyntaxChecking_Interrupted
                        : (SyntaxCheckingApplier.begin_line == 0)
                            ? SyntaxChecking_IsPerfect
                            : SyntaxChecking_DoingFull;

                    wx=Win.x; wy=Win.y;
                    VisRender();
                }
                if(wx != Win.x || wy != Win.y)
                    { wx=Win.x; wy=Win.y; VisRender(); }
            }
            VisRenderStatus();
            VisSoftCursor(0);
        #ifdef __BORLANDC__
            if(SyntaxCheckingNeeded == SyntaxChecking_IsPerfect
            || SyntaxCheckingNeeded != SyntaxChecking_DoingFull)
            {
                if(SyntaxCheckingNeeded == SyntaxChecking_IsPerfect)
                    Cycles_Adjust(-1);
                _asm { hlt }
            }
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

#define AllCursors() \
    do { int cn; \
         for(cn=0; cn<NumCursors; ++cn) \
             { Anchor& c = SavedCursors[cn]; o(); } \
    } while(0)
#define AlmostAllCursors() \
    do { int cn; \
         for(cn=0; cn<MaxSavedCursors; ++cn) \
             { Anchor& c = SavedCursors[cn]; o(); } \
    } while(0)
#define TheOtherCursors() \
    do { int cn; \
         for(cn=MaxSavedCursors; cn<NumCursors; ++cn) \
             { Anchor& c = SavedCursors[cn]; o(); } \
    } while(0)

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
        while(n_delete >= EditLines[y].size() - x
           && y+1+n_lines_deleted < EditLines.size())
        {
            ++n_lines_deleted;
            #define o()  if(c.y == y+n_lines_deleted) { c.y = y; c.x += EditLines[y].size(); }
            AllCursors();
            #undef o

            EditLines[y].insert( EditLines[y].end(),
                EditLines[y+n_lines_deleted].begin(),
                EditLines[y+n_lines_deleted].end() );
        }
        if(n_lines_deleted > 0)
        {
            #define o() if(c.y > y) c.y -= n_lines_deleted
            AllCursors();
            #undef o
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
        #define o() if(c.y == y && c.x > x+n_delete) c.x -= n_delete; \
               else if(c.y == y && c.x > x) c.x = x
        AllCursors();
        #undef o
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
            #define o() if(c.y == y && c.x >= x) { c.y += insert_newline_count; c.x -= x; } \
                   else if(c.y > y) { c.y += insert_newline_count; }
            AlmostAllCursors();
            #undef o
            #define o() if(c.y == y && c.x > x) { c.y += insert_newline_count; c.x -= x; } \
                   else if(c.y > y) { c.y += insert_newline_count; }
            TheOtherCursors();
            #undef o
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
                #define o() if(c.y == y && c.x >= x) c.x += n_inserted
                AlmostAllCursors();
                #undef o
                // For block begin & end markers, don't move them
                // if they are right at the cursor's location.
                #define o() if(c.y == y && c.x > x) c.x += n_inserted
                TheOtherCursors();
                #undef o
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
    unsigned firsty = BlockBegin.y, lasty = BlockEnd.y;
    if(BlockEnd.x == 0) lasty -= 1;

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
        if(BlockBegin.x > 0) BlockBegin.x += offset;
        if(BlockEnd.x   > 0) BlockEnd.x   += offset;
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
        if(BlockBegin.x >= outdent) BlockBegin.x -= outdent;
        if(BlockEnd.x   >= outdent) BlockEnd.x   -= outdent;
    }
    SyntaxCheckingNeeded = SyntaxChecking_DidEdits;
}

void GetBlock(WordVecType& block)
{
    for(unsigned y=BlockBegin.y; y<=BlockEnd.y; ++y)
    {
        unsigned x0 = 0, x1 = EditLines[y].size();
        if(y == BlockBegin.y) x0 = BlockBegin.x;
        if(y == BlockEnd.y)   x1 = BlockEnd.x;
        block.insert(block.end(),
            EditLines[y].begin() + x0,
            EditLines[y].begin() + x1);
    }
}

void FindPair()
{
    int           PairDir  = 0;
    unsigned char PairChar = 0;
    unsigned char PairColor = EditLines[Cur.y][Cur.x] >> 8;
    switch(EditLines[Cur.y][Cur.x] & 0xFF)
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
    unsigned testx = Cur.x, testy = Cur.y;
    if(PairDir > 0)
        for(;;)
        {
            if(++testx >= EditLines[testy].size())
                { testx=0; ++testy; if(testy >= EditLines.size()) return; }
            if((EditLines[testy][testx] >> 8) != PairColor) continue;
            unsigned char c = EditLines[testy][testx] & 0xFF;
            if(balance == 0 && c == PairChar) { Cur.x = testx; Cur.y = testy; return; }
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
            if(balance == 0 && c == PairChar) { Cur.x = testx; Cur.y = testy; return; }
            if(c == '{' || c == '[' || c == '(') ++balance;
            if(c == '}' || c == ']' || c == ')') --balance;
        }
}

int SelectFont()
{
    struct opt
    {
        unsigned char px, py;
        unsigned w[5], h[5], cx[5],cy[5],wid[5];
    };
    opt options[] =
    {
        { 8, 8, {0},{0},{0},{0},{0} }, { 9,  8, {0},{0},{0},{0},{0} },
        { 8,10, {0},{0},{0},{0},{0} }, { 9, 10, {0},{0},{0},{0},{0} },
        { 8,12, {0},{0},{0},{0},{0} }, { 9, 12, {0},{0},{0},{0},{0} },
        { 8,14, {0},{0},{0},{0},{0} },
        { 8,15, {0},{0},{0},{0},{0} }, { 9, 15, {0},{0},{0},{0},{0} },
        { 8,16, {0},{0},{0},{0},{0} }, { 9, 16, {0},{0},{0},{0},{0} },
        { 8,19, {0},{0},{0},{0},{0} }, { 9, 19, {0},{0},{0},{0},{0} },
        { 8,32, {0},{0},{0},{0},{0} }, { 9, 32, {0},{0},{0},{0},{0} },
        {16,32, {0},{0},{0},{0},{0} }
    };
    if(VidCellHeight == 8 || VidCellHeight == 12 || VidCellHeight == 14)
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
                  if(o.h[m] == 48 && o.py != 15) o.h[m] = 50;
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
        StatusLine[0] = '\0';

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
        else if(c == CTRL('A')) goto hom;
        else if(c == CTRL('B')) goto left;
        else if(c == CTRL('F')) goto right;
        else if(c == CTRL('E')) goto end;
        else if(c == CTRL('U')) goto pgup;
        else if(c == CTRL('V')) goto pgdn;
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
                case 'K': left: if(sel_x>0) --sel_x; else { sel_x=4; goto up; } break;
                case 'M': right: if(sel_x<4) ++sel_x; else { sel_x=0; goto dn; } break;
                case 0x47: hom: if(sel_x > 0) sel_x=0; else sel_y=0; break;
                case 0x4F: end: if(sel_x < 4) sel_x=4; else sel_y=noptions-1; break;
                case 0x49: pgup: sel_y = 0; break;
                case 0x51: pgdn: sel_y = noptions-1; break;
                case 0x42: goto gotesc; // F8
                default: goto rewait;
            }
        else goto rewait;
    }
    if(sel_y == -1)
    {
        StatusLine[0] = '\0';
        return 0;
    }
    dblw = wdblset[sel_x];
    dblh = hdblset[sel_x];
    VidCellHeight = options[sel_y].py;
    VidW          = options[sel_y].w[sel_x];
    VidH          = options[sel_y].h[sel_x];
    use9bit = (options[sel_y].px == 9) || (options[sel_y].px == 18);
    FatMode = options[sel_y].px >= 16;
    if(VidCellHeight != 8) DCPUpalette = 0;
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
        if(c == CTRL('C')
        || c == 'N' || c == 'n') { decision=0; break; }
        if(c == 0) getch();
    }
    StatusLine[0] = '\0';
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
            case CTRL('C'):
                StatusLine[0] = '\0';
                return 0;
            case CTRL('B'): goto kb_lt;
            case CTRL('F'): goto kb_rt;
            case CTRL('A'): goto kb_hom;
            case CTRL('E'): goto kb_end;
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
            case CTRL('H'):
                if(curPos <= 0) break;
                strcpy(data+curPos-1, data+curPos);
                --curPos;
                if(firstPos > 0) --firstPos;
                break;
            case CTRL('D'): goto kb_del;
            case CTRL('Y'): curPos = 0; data[0] = '\0'; break;
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
void LineAskGo() // Go to line
{
    unsigned DimY = VidH-1;
    char* line = 0;
    char Buf[64] = "";
    //sprintf(Buf, "%u", Cur.y + 1);
    int decision = PromptText("Goto line:", Buf, &line);
    if(!decision || !line || !*line)
    {
        if(line) free(line);
        return;
    }
    unsigned oldy = Cur.y;
    Cur.x = 0;
    Cur.y = atoi(line) - 1;
    free(line);
    //Win.y = (Cur.y > DimY/2) ? Cur.y - (DimY>>1) : 0;
    if(Win.y > Cur.y || Win.y+DimY <= Cur.y)
    {
        Win.y = Cur.y > oldy
            ? (Cur.y > (DimY/2)
                ? Cur.y - (DimY/2)
                : 0)
            : Cur.y;
    }
    Win.x = 0;
    VisRenderStatus();
    VisRender();
}
void ResizeAsk() // Ask for new screen dimensions
{
    char* line = 0;
    char Buf[64] = "";
    int neww=0, newh=0;
    sprintf(Buf, "%u %u", VidW, VidH);
    int decision = PromptText("Enter new screen dimensions:", Buf, &line);
    int n = line ? sscanf(line, "%d %d", &neww,&newh) : 0;
    if(!decision || !line || !*line || n != 2 || neww < 8 || neww > 160 || newh < 4 || newh > 132)
    {
        if(line) free(line);
        return;
    }
    VidW = neww;
    VidH = newh;
}

void QBcompileRun()
{
    // To compile properly, need DECLARE statements.
    // Let's go the easier way and use QuickBASIC
    VisPutCursorAt(0, VidH-1);
#if 1
    char Cmd1[1024];
    sprintf(Cmd1, "/RUN %s /AH/G", CurrentFileName);
    if(printf("qb %s\n", Cmd1) >= 0
    && spawnlp(P_WAIT, "qb",   "qb", Cmd1, NULL) == 0)
    {
        //
    }
#else
    static const char objfn[] = "tmp.obj";
    static const char exefn[] = "tmp.exe";
    char Cmd1[1024];
    char Cmd2[1024];
    int has_resume_lineno = 0;
    int has_resume_other  = 0;
    int has_event_triggers = 0;

    sprintf(Cmd1, "%s,%s,NUL/ah%s%s%s%s%s",
        CurrentFileName,
        objfn,
        0 ? "/d" : "", // debugging
        has_resume_lineno ? "/e" : "", // has RESUME <linenumber>
        1 ? "/o" : "", // standalone
        has_event_triggers ? "/v" : "", // has ON TIMER, ON PLAY etc. event traps
        has_resume_other ? "/x" : "" // has RESUME, RESUME NEXT, or RESUME 0
    );
    sprintf(Cmd2, "%s,%s,NUL,bcom45.lib,,/NOE",
        objfn,
        exefn);

    if(printf("bc %s\n", Cmd1) >= 0
    && spawnlp(P_WAIT, "bc",   "bc", Cmd1, NULL) == 0
    && printf("link %s\n", Cmd2) >= 0
    && spawnlp(P_WAIT, "link", "link", Cmd2, NULL) == 0)
    {
        spawnl(P_WAIT, (char*) exefn, (char*) exefn, NULL);
    }
#endif
}

int main(int argc, char**argv)
{
  #if 1
    // Set mode (for dosbox recording)
    VgaSetCustomMode(80,25,16,1,0,0, 1);// columns = 1;
  #endif

#ifdef __BORLANDC__
    InstallMario();
#endif
    Syntax.Parse("qb.jsf");
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

    unsigned long StatusLineProtection = MarioTimer + 200u;

    for(;;)
    {
        WaitInput();
        unsigned c = getch();

#ifdef __BORLANDC__
        int shift = 3 & (*(char*)MK_FP(0x40,0x17));
#else
        int shift = 0;
#endif
        int wasbegin = Cur.x==BlockBegin.x && Cur.y==BlockBegin.y;
        int wasend   = Cur.x==BlockEnd.x && Cur.y==BlockEnd.y;
        unsigned WasX = Cur.x, WasY = Cur.y;
        int dragalong=0;
        if(StatusLine[0] && StatusLineProtection < MarioTimer)
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
                unsigned offset = Cur.y-Win.y;
                Cur.y += DimY-3;
                if(Cur.y >= EditLines.size()) Cur.y = EditLines.size()-1;
                Win.y = (Cur.y > offset) ? Cur.y-offset : 0;
                /*if(Win.y + DimY > EditLines.size()
                && EditLines.size() > DimY) Win.y = EditLines.size()-DimY;*/
                if(shift && ENABLE_DRAG) dragalong = 1;
                break;
            }
            case CTRL('U'): // ctrl-U
            {
            pgup:;
                unsigned offset = Cur.y - Win.y;
                if(Cur.y > DimY-3) Cur.y -= DimY-3; else Cur.y = 0;
                Win.y = (Cur.y > offset) ? Cur.y-offset : 0;
                if(shift && ENABLE_DRAG) dragalong = 1;
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
            case CTRL('W'):
                if(Win.y > 0) --Win.y;
                StatusLine[0] = '\0';
                break;
            case CTRL('Z'):
                if(Win.y+1/*+DimY*/ < EditLines.size()) ++Win.y;
                if(Cur.y < Win.y) Cur.y = Win.y;
                StatusLine[0] = '\0';
                break;
            case CTRL('K'):
            {
                WaitingCtrl='K';
                WaitInput();
                WaitingCtrl=0;
                switch(getch())
                {
                    case 'b': case 'B': case CTRL('B'): // mark begin
                        BlockBegin.x = Cur.x; BlockBegin.y = Cur.y;
                        VisRender();
                        break;
                    case 'k': case 'K': case CTRL('K'): // mark end
                        BlockEnd.x = Cur.x; BlockEnd.y = Cur.y;
                        VisRender();
                        break;
                    case 'm': case 'M': case CTRL('M'): // move block
                    {
                        WordVecType block, empty;
                        GetBlock(block);
                        PerformEdit(BlockBegin.x,BlockBegin.y, block.size(), empty);
                        // Note: ^ Assumes Cur.x,Cur.y get updated here.
                        BlockEnd.x = Cur.x; BlockEnd.y = Cur.y;
                        unsigned x = Cur.x, y = Cur.y;
                        PerformEdit(Cur.x,Cur.y, InsertMode?0u:block.size(), block);
                        BlockBegin.x = x; BlockBegin.y = y;
                        break;
                    }
                    case 'c': case 'C': case CTRL('C'): // paste block
                    {
                        WordVecType block;
                        GetBlock(block);
                        unsigned x = Cur.x, y = Cur.y;
                        PerformEdit(Cur.x,Cur.y, InsertMode?0u:block.size(), block);
                        BlockBegin.x = x; BlockBegin.y = y;
                        BlockEnd.x = Cur.x; BlockEnd.y = Cur.y;
                        break;
                    }
                    case 'y': case 'Y': case CTRL('Y'): // delete block
                    {
                        WordVecType block, empty;
                        GetBlock(block);
                        PerformEdit(BlockBegin.x,BlockBegin.y, block.size(), empty);
                        BlockEnd.x = BlockBegin.x;
                        BlockEnd.y = BlockEnd.y;
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
                    case 'n': case 'N': case CTRL('N'): // new file
                        FileNew();
                        Syntax.Parse("conf.jsf");
                        break;
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
                        unsigned charcode = EditLines[Cur.y][Cur.x] & 0xFF;
                        sprintf(StatusLine,
                            VidW >= 60
                            ? "Character '%c': hex=%02X, decimal=%d, octal=%03o"
                            : "Character '%c': 0x%02X = %d = '\\0%03o'",
                            charcode, charcode, charcode, charcode);
                        break;
                    }
                    case 'l': case 'L': case CTRL('L'): // ask line number and goto
                    {
                        LineAskGo();
                        break;
                    }
                    case '\'': // Insert literal character
                    {
                        c = getch();
                        WordVecType txtbuf(1, 0x0700 | (c & 0xFF));
                        if( (c & 0xFF) == 0)
                        {
                            txtbuf.push_back( 0x0700 | (c >> 8) );
                        }
                        PerformEdit(Cur.x,Cur.y, InsertMode?0u:1u, txtbuf);
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
                VgaGetMode();
                VisSetCursor();
                VisRender();
                break;
            case CTRL('G'):
                FindPair();
                if(Cur.y < Win.y || Cur.y >= Win.y+DimY)
                    Win.y = Cur.y > DimY/2 ? Cur.y - DimY/2 : 0;
                break;
            case CTRL('T'): goto askmode;
            case 0:;
            {
                switch(getch())
                {
                    case 'H': // up
                        if(Cur.y > 0) --Cur.y;
                        if(Cur.y < Win.y) Win.y = Cur.y;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 'P': // down
                        if(Cur.y+1 < EditLines.size()) ++Cur.y;
                        if(Cur.y >= Win.y+DimY-3) Win.y = Cur.y - DimY+3;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 0x47: // home
                    {
                        #define k_home() do { \
                            unsigned x = 0; \
                            while(x < EditLines[Cur.y].size() \
                               && (EditLines[Cur.y][x] & 0xFF) == ' ') ++x; \
                            if(Cur.x == x) Cur.x = 0; else Cur.x = x; } while(0)
                    home:
                        k_home();
                        Win.x = 0;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 0x4F: // end
                        #define k_end() do { \
                            Cur.x = EditLines[Cur.y].size(); \
                            if(Cur.x > 0 && (EditLines[Cur.y].back() & 0xFF) == '\n') \
                                --Cur.x; /* past LF */ } while(0)
                    end:
                        k_end();
                        Win.x = 0;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 'K': // left
                    {
                        #define k_left() do { \
                            if(Cur.x > EditLines[Cur.y].size()) \
                                Cur.x = EditLines[Cur.y].size()-1; \
                            else if(Cur.x > 0) --Cur.x; \
                            else if(Cur.y > 0) { --Cur.y; k_end(); } } while(0)
                    lt_key:
                        k_left();
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 'M': // right
                    {
                        #define k_right() do { \
                            if(Cur.x+1 < EditLines[Cur.y].size()) \
                                ++Cur.x; \
                            else if(Cur.y+1 < EditLines.size()) \
                                { Cur.x = 0; ++Cur.y; } } while(0)
                    rt_key:
                        k_right();
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 0x73: // ctrl-left (go left on word boundary)
                    {
                        k_left();
                        do k_left();
                        while( (Cur.x > 0 || Cur.y > 0)
                            && isalnum(EditLines[Cur.y][Cur.x]&0xFF));
                        k_right();
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 0x74: // ctrl-right (go right on word boundary)
                    {
                        while( Cur.y+1 < EditLines.size()
                            && Cur.x < EditLines[Cur.y].size()
                            && isalnum(EditLines[Cur.y][Cur.x]&0xFF) )
                            k_right();
                        do k_right();
                        while( Cur.y+1 < EditLines.size()
                            && Cur.x < EditLines[Cur.y].size()
                            && !isalnum(EditLines[Cur.y][Cur.x]&0xFF) );
                        if(shift && ENABLE_DRAG) dragalong = 1;
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
                        Cur.y = Win.y = 0;
                        Cur.x = Win.x = 0;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 0x76: // ctrl-pgdn = goto end of file
                    ctrlpgdn:
                        Cur.y = EditLines.size()-1;
                        Win.y = 0;
                        if(Cur.y >= Win.y+DimY-3) Win.y = Cur.y - DimY+3;
                        goto end;
                    case 0x77: // ctrl-home = goto beginning of window (vertically)
                        Cur.y = Win.y;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 0x75: // ctrl-end = goto end of window (vertically)
                        Cur.y = Win.y + VidH-4;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        // Hide the status line
                        StatusLine[0] = '\0';
                        break;
                    case 0x53: // delete
                    delkey:
                    {
                        unsigned eol_x = EditLines[Cur.y].size();
                        if(eol_x > 0 && (EditLines[Cur.y].back() & 0xFF) == '\n') --eol_x;
                        if(Cur.x > eol_x) { Cur.x = eol_x; break; } // just do end-key
                        WordVecType empty;
                        PerformEdit(Cur.x,Cur.y, 1u, empty);
                        WasAppend = 1;
                        break;
                    }
                    case 0x54: shiftF1:
                               ResizeAsk(); goto newmode;
                    case 0x3B: if(shift) goto shiftF1;
                               VidH -= 1; goto newmode; // F1
                    case 0x3C: VidH += 1; goto newmode; // F2
                    case 0x3D: VidW -= 2; goto newmode; // F3
                    case 0x3E: VidW += 2; goto newmode; // F4
                    case 0x3F:
               #if 0
                        use9bit = !use9bit; goto newmode; // F5
               #else
                        // QBASIC COMPILE
                    {
                        DeInstallMario();
                        QBcompileRun();
                        InstallMario();
                        goto newmode; // reset the display mode
                    }
               #endif
                    case 0x40: if(shift) goto shiftF6;
                        if(dblw) { dblw=0; VidW*=2; }
                        else     { dblw=1; VidW/=2; VidW&=~1; }
                        goto newmode; // F6
                    case 0x59: shiftF6:
                               dblw = !dblw; goto newmode; // shift-F6
                    case 0x41: if(shift) goto shiftF7;
                        if(dblh) { dblh=0; if(VidCellHeight==8 || VidCellHeight==16)
                                               VidCellHeight *= 2;
                                           else
                                               VidH*=2; }
                        else    { dblh=1; if(VidCellHeight==16 || (VidCellHeight==32 && !FatMode))
                                               VidCellHeight /= 2;
                                           else
                                               VidH/=2; }
                        goto newmode; // F7
                    case 0x5A: shiftF7:
                               dblh = !dblh; goto newmode; // shift-F7
                    case 0x5B: // shift-F8
                    case 0x42: // F8
                    askmode:
                        if(SelectFont())
                        {
                        newmode:
                            if(VidW > 240) VidW = 240;
                            if(VidH > 127) VidH = 127;
                            VgaSetCustomMode(VidW,VidH, VidCellHeight,
                                             use9bit, dblw, dblh,
                                             columns);
                            char FPSstr[64] = "";
                            if(VidW >= 40)
                            {
                                long fpsval = VidFPS*1000.0 + 0.5;
                                sprintf(FPSstr, "; fps=%ld.%03ld", fpsval/1000, fpsval%1000);
                                // ^ For some reason, fltform print doesn't work
                            }
                            sprintf(StatusLine,
                                "%s: %ux%u %s %ux%u (%ux%u)%s",
                                    VidW < 53 ? "Mode" : "Selected text mode",
                                    VidW,VidH,
                                    VidW < 44 ? "w/" : "with",
                                    (use9bit ? 9 : 8) * (FatMode?2:1),
                                    VidCellHeight,
                                    VidW * (use9bit ? 9 : 8) * (1+dblw) * (FatMode?2:1),
                                    VidH * VidCellHeight * (1+dblh),
                                    FPSstr);
                        }
                        VisSetCursor();
                        VisRender();
                        if(C64palette)
                        {
                            long fpsval = VidFPS*1000.0 + 0.5;
                            char res[64];
                            sprintf(res, "%ux%u, %ld.%03ld fps", VidW,VidH, fpsval/1000, fpsval%1000);
                            sprintf(StatusLine, "READY%*s", VidW-5, res);
                        }
                        VisRenderStatus();
                        StatusLineProtection = MarioTimer + 200u;
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
                            long fpsval = VidFPS*1000.0 + 0.5;
                            char res[64];
                            sprintf(res, "%ux%u, %ld.%03ld fps", VidW,VidH, fpsval/1000, fpsval%1000);
                            sprintf(StatusLine, "READY%*s", VidW-5, res);
                        }
                        VisRender();
                        break;
                    case 0x2C: TryUndo(); break; // alt+Z
                    case 0x15: TryRedo(); break; // alt+Y
                    case 0x13: TryRedo(); break; // alt+R
                    case 0x78: CurrentCursor=0; break; // alt+1
                    case 0x79: CurrentCursor=1; break; // alt+2
                    case 0x7A: CurrentCursor=2; break; // alt+3
                    case 0x7B: CurrentCursor=3; break; // alt+4
                }
                break;
            }
            case CTRL('Y'): // erase line
            {
                Cur.x = 0;
                WordVecType empty;
                PerformEdit(Cur.x,Cur.y, EditLines[Cur.y].size(), empty);
                break;
            }
            case CTRL('H'): // backspace = left + delete
            {
                WordVecType empty;
                unsigned nspaces = 0;
                while(nspaces < EditLines[Cur.y].size()
                   && (EditLines[Cur.y][nspaces] & 0xFF) == ' ') ++nspaces;
                if(nspaces > 0 && Cur.x == nspaces)
                {
                    nspaces = 1 + (Cur.x-1) % TabSize;
                    Cur.x -= nspaces;
                    PerformEdit(Cur.x,Cur.y, nspaces, empty);
                }
                else
                {
                    if(Cur.x > 0) --Cur.x;
                    else if(Cur.y > 0)
                    {
                        --Cur.y;
                        Cur.x = EditLines[Cur.y].size();
                        if(Cur.x > 0) --Cur.x; // past LF
                    }
                    PerformEdit(Cur.x,Cur.y, 1u, empty);
                }
                WasAppend = 1;
                break;
            }
            case CTRL('D'):
                goto delkey;
            case CTRL('I'):
            {
                unsigned nspaces = TabSize - Cur.x % TabSize;
                WordVecType txtbuf(nspaces, 0x1720);
                PerformEdit(Cur.x,Cur.y, InsertMode?0u:nspaces, txtbuf);
                break;
            }
            case CTRL('M'): // enter
            case CTRL('J'): // ctrl+enter
            {
                unsigned nspaces = 0;
                if(InsertMode)
                {
                    // Autoindent only in insert mode
                    while(nspaces < EditLines[Cur.y].size()
                       && (EditLines[Cur.y][nspaces] & 0xFF) == ' ') ++nspaces;
                    if(Cur.x < nspaces) nspaces = Cur.x;
                }
                WordVecType txtbuf(nspaces + 1, 0x1720);
                txtbuf[0] = 0x170A; // newline
                PerformEdit(Cur.x,Cur.y, InsertMode?0u:1u, txtbuf);
                //Win.x = 0;
                WasAppend = 1;
                break;
            }
            default:
            {
                WordVecType txtbuf(1, 0x1700 | c);
                PerformEdit(Cur.x,Cur.y, InsertMode?0u:1u, txtbuf);
                WasAppend = 1;
                break;
            }
        }
        UndoAppendOk = WasAppend;
        if(dragalong)
        {
            int w=0;
            if(wasbegin || !wasend) { BlockBegin.x=Cur.x; BlockBegin.y=Cur.y; w=1; }
            if(!wasbegin)           { BlockEnd.x=Cur.x; BlockEnd.y=Cur.y; w=1; }
            if( !wasbegin && !wasend
            && BlockBegin.x==BlockEnd.x
            && BlockBegin.y==BlockEnd.y) { BlockEnd.x=WasX; BlockEnd.y=WasY; }
            if(BlockBegin.y > BlockEnd.y
            || (BlockBegin.y == BlockEnd.y && BlockBegin.x > BlockEnd.x))
               {{ unsigned tmp = BlockBegin.y; BlockBegin.y=BlockEnd.y; BlockEnd.y=tmp; }
                { unsigned tmp = BlockBegin.x; BlockBegin.x=BlockEnd.x; BlockEnd.x=tmp; }}
            if(w) VisRender();
        }
    }
exit:;
    Cur.x = 0; Cur.y = Win.y + VidH; InsertMode = 1;
    if(FatMode || C64palette)
    {
        FatMode=0;
        C64palette=0;
        VgaSetCustomMode(VidW,VidH, VidCellHeight,
                         use9bit, dblw, dblh, columns);
    }
    VisSetCursor();
#ifdef __BORLANDC__
    DeInstallMario();
#endif
    exit(0);
    return 0;
}
