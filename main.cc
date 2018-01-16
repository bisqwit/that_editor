/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef __BORLANDC__
# include <process.h> // For Cycles adjust on DOSBOX
#endif
#ifdef __DJGPP__
# include <dos.h>
# include <dpmi.h>
#endif

#define CTRL(c) ((c) & 0x1F)

static const int ENABLE_DRAG = 0;

volatile unsigned long MarioTimer = 0;
static unsigned long chars_file  = 0;
static unsigned long chars_typed = 0;

static int use9bit, dblw, dblh, columns=1;

static inline int isalnum_(unsigned char c)
{
    return isalnum(c) || c == '_';
}
static inline int ispunct_(unsigned char c)
{
    return !isspace(c) && !isalnum_(c);
}

// Return just the filename part of pathfilename
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

#include "kbhit.hh"

#include "vec_c.hh"
#include "vec_s.hh"
#include "vec_lp.hh"

#include "jsf.hh"
#include "vga.hh"

#include "cpu.h"

const unsigned UnknownColor = 0x2400;

char StatusLine[256] =
"Ad-hoc programming editor - (C) 2011-03-08 Joel Yliluoma";

LongPtrVecType EditLines;

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
    LongVecType editline;
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

    chars_file = 0;
    for(size_t a=0; a<EditLines.size(); ++a)
        chars_file += EditLines[a].size();
}
void FileNew()
{
    Win = Cur = Anchor();
    EditLines.clear();
    LongVecType emptyline;
    emptyline.push_back('\n' | UnknownColor);
    EditLines.push_back(emptyline);
    EditLines.push_back(emptyline);
    EditLines.push_back(emptyline);

    if(CurrentFileName) free(CurrentFileName);
    CurrentFileName = 0;
    chars_file = 3; // Three newlines
}
struct ApplyEngine: public JSF::Applier
{
    int finished;
    unsigned nlinestotal, nlines;
    size_t x,y, begin_line;
    unsigned pending_recolor_distance, pending_recolor;
    unsigned long pending_attr;
    ApplyEngine()
        { Reset(0); }
    void Reset(size_t line)
        { x=0; y=begin_line=line; finished=0; nlinestotal=nlines=0;
          pending_recolor=0;
          pending_attr   =0;
        }
    virtual cdecl int Get(void)
    {
        if(y >= EditLines.size() || EditLines[y].empty())
        {
            finished = 1;
            FlushColor();
            return -1;
        }
        int ret = EditLines[y][x] & 0xFF;
        if(ret == '\n')
        {
            if(kbhit()) { return -1; }
            ++nlines;
            if((nlines >= VidH)
            || (nlinestotal > Win.y + VidH && nlines >= 4))
                { nlines=0; FlushColor(); return -1; }
            ++nlinestotal;
        }
        pending_recolor_distance += 1;
        ++x;
        if(x == EditLines[y].size()) { x=0; ++y; }
        //fprintf(stdout, "Gets '%c'\n", ret);
        return ret;
    }
    /* attr     = Attribute to set
     * n        = Number of last characters to apply that attribute for
     * distance = Extra number of characters to count and skip
     */
    virtual cdecl void Recolor(register unsigned distance, register unsigned n, register unsigned long attr)
    {
        /* Flush the previous req, unless this new req is a super-set of the previous request */
        if(pending_recolor > 0)
        {
            register unsigned old_apply_begin = pending_recolor + pending_recolor_distance;
            register unsigned old_apply_end   = pending_recolor_distance;
            register unsigned new_apply_begin = distance + n;
            register unsigned new_apply_end   = distance;
            if(new_apply_begin < old_apply_begin || new_apply_end > old_apply_end)
            {
                FlushColor();
            }
        }
        pending_recolor_distance = distance;
        pending_recolor          = n;
        pending_attr             = attr << 8;
    }
private:
    void FlushColor()
    {
        register unsigned dist      = pending_recolor_distance;
        register unsigned n         = pending_recolor;
        register unsigned long attr = pending_attr;
        if(n > 0)
        {
            //fprintf(stdout, "Recolors %u as %02X\n", n, attr);
            size_t px=x, py=y;
            LongVecType* line = &EditLines[py];
            for(n += dist; n > 0; --n)
            {
                if(px == 0) { if(!py) break; line = &EditLines[--py]; px = line->size()-1; }
                else --px;
                if(dist > 0)
                    --dist;
                else
                {
                    unsigned long& w = (*line)[px];
                    w = (w & 0xFF) | attr;
                }
            }
        }
        pending_recolor          = 0;
        pending_recolor_distance = 0;
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
#elif defined(__DJGPP__)
    { REGS r{}; r.h.ah = 3; r.h.bh = 0; int86(0x10, &r, &r); cux = r.h.dl; cuy = r.h.dh; }
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
            #ifdef __GNUC__
            [[fallthrough]];
            #endif
        case 0:
            *cursor_location = ((evacuation&0xF00)<<4)|evacuation;
            break;
        case 30:
            *cursor_location = evacuation;
    }
}
void VisPutCursorAt(unsigned cx,unsigned cy)
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

    unsigned size = InsertMode ? (VidCellHeight-2) : (VidCellHeight*2/8);
    size = (size << 8) | (VidCellHeight-1);
    if(C64palette) size = 0x3F3F;
    #ifdef __DJGPP__
    unsigned addr = cux + cuy*VidW;
    _farpokeb(_dos_ds, 0x450, (cuy<<8) + cux);
    outport(0x3D4, 0x0E + (addr&0xFF00));
    outport(0x3D4, 0x0F + ((addr&0xFF)<<8));
    // Set cursor shape: lines-4 to lines-3, or 6 to 7
    outport(0x3D4, 0x0A + ((VidCellHeight>8 ? VidCellHeight-4 : 6) << 8));
    outport(0x3D4, 0x0B + ((VidCellHeight>8 ? VidCellHeight-3 : 7) << 8));
    #else
    _asm { mov ah, 2; mov bh, 0; mov dh, cuy; mov dl, cux; int 0x10 }
    _asm { mov ah, 1; mov cx, size; int 0x10 }
    #endif
    CursorCounter=0;
#else
    cx=cx; cy=cy;
#endif
#ifdef __DJGPP__
    _farpokeb(_dos_ds, 0x450, cx);
    _farpokeb(_dos_ds, 0x451, cy);
#else
    *(unsigned char*)MK_FP(0x40,0x50) = cx;
    *(unsigned char*)MK_FP(0x40,0x51) = cy;
#endif
}
void VisSetCursor()
{
    unsigned cx = Win.x > Cur.x ? 0 : Cur.x-Win.x;       if(cx >= VidW) cx = VidW-1;
    unsigned cy = Win.y > Cur.y ? 1 : Cur.y-Win.y; ++cy; if(cy >= VidH) cy = VidH-1;
    VisPutCursorAt(cx,cy);
}

static long CYCLES_Current = 80000l;
static long CYCLES_Goal    = 80000l;
static int Cycles_Trend = 0;

static void Cycles_Adjust(int direction)
{
    Cycles_Trend = direction;
}
void Cycles_Check()
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
        CYCLES_Current = CYCLES_Goal;

        #ifdef __BORLANDC__
        char Buf[64];
        sprintf(Buf, " CYCLES=%ld", CYCLES_Current);
        unsigned psp = getpsp();
        strcpy( (char*) MK_FP(psp, 0x80), Buf);
        // FIXME: THIS METHOD OF INVOKING CONFIG.COM DEPENDS ON EXACT DOSBOX VERSION.
        // ALSO, IT WILL NOT WORK IF RUNNING E.G. UNDER FREEDOS OR DPMI...
        _asm { db 0xFE,0x38,0x06,0x00 }
        #elif defined(__DJGPP__)

        /* HUGE WARNING: THIS *REQUIRES* A PATCHED DOSBOX,
         * UNPATCHED DOSBOXES WILL TRIGGER AN EXCEPTION HERE */
        __asm__ volatile("movl %0, %%tr2" : : "a"(CYCLES_Current));

        #endif
    }
    Cycles_Trend = 0;
}

struct ColorSlideCache
{
    enum { MaxWidth = 192 };
    const unsigned char*  const colors;
    const unsigned short* const color_positions;
    const unsigned              color_length;
    unsigned cached_width;
    unsigned short cache_color[MaxWidth];
    unsigned char cache_char[MaxWidth];

public:
    ColorSlideCache(const unsigned char* c, const unsigned short* p, unsigned l)
        : colors(c), color_positions(p), color_length(l), cached_width(0) {}

    void SetWidth(unsigned w)
    {
        if(w == cached_width) return;
        cached_width = w;
        unsigned char first=0;
        { memset(cache_char, 0, sizeof(cache_char)); memset(cache_color, 0, sizeof(cache_color)); }
        for(unsigned x=0; x<w && x<MaxWidth; ++x)
        {
            unsigned short cur_position = (((unsigned long)x) << 16u) / w;
            while(first < color_length && color_positions[first] <= cur_position) ++first;
            unsigned long  next_position=0;
            unsigned char  next_value=0;
            if(first < color_length)
                { next_position = color_positions[first]; next_value = colors[first]; }
            else
                { next_position = 10000ul; next_value = colors[color_length-1]; }

            unsigned short prev_position=0;
            unsigned char  prev_value   =next_value;
            if(first > 0)
                { prev_position = color_positions[first-1]; prev_value = colors[first-1]; }

            float position = (cur_position - prev_position) / float(next_position - prev_position );
            //static const unsigned char chars[4] = { 0x20, 0xB0, 0xB1, 0xB2 };
            //register unsigned char ch = chars[unsigned(position*4)];
            static const unsigned char chars[2] = { 0x20, 0xDC };
            register unsigned char ch = chars[unsigned(position*2)];
            //unsigned char ch = 'A';
            if(prev_value == next_value || ch == 0x20) { ch = 0x20; next_value = 0; }
            cache_char[x] = ch;
            cache_color[x] = prev_value | (next_value << 8u);
            //cache[x] = 0x80008741ul;
        }
    }
    inline void Get(unsigned x, unsigned char& ch, unsigned char& c1, unsigned char& c2) const
    {
        register unsigned short tmp = cache_color[x];
        ch = cache_char[x];
        c1 = tmp;
        c2 = tmp >> 8u;
    }
};
/*
        float xscale = x * (sizeof(slide)-1) / float(StatusWidth-1);
        unsigned c1 = slide[(unsigned)( xscale + Bayer[0*8 + (x&7)] )];
        unsigned c2 = slide[(unsigned)( xscale + Bayer[1*8 + (x&7)] )];
        unsigned char ch = 0xDC;
*/

static const unsigned char  slide1_colors[21] = {6,73,109,248,7,7,7,7,7,248,109,73,6,6,6,36,35,2,2,28,22};
static const unsigned short slide1_positions[21] = {0u,1401u,3711u,6302u,7072u,8192u,16384u,24576u,32768u,33889u,34659u,37250u,39560u,40960u,49152u,50903u,53634u,55944u,57344u,59937u,63981u};
static const unsigned char  slide2_colors[35] = {248,7,249,250,251,252,188,253,254,255,15,230,229,228,227,11,227,185,186,185,179,143,142,136,100,94,58,239,238,8,236,235,234,233,0};
static const unsigned short slide2_positions[35] = {0u,440u,1247u,2126u,3006u,3886u,4839u,5938u,6965u,8064u,9750u,12590u,15573u,18029u,19784u,21100u,24890u,27163u,30262u,35051u,35694u,38054u,40431u,41156u,46212u,46523u,50413u,52303u,53249u,54194u,56294u,58815u,61335u,63856u,64696u};

static ColorSlideCache slide1(slide1_colors, slide1_positions, sizeof(slide1_colors));
static ColorSlideCache slide2(slide2_colors, slide2_positions, sizeof(slide2_colors));

void VisRenderStatus()
{
    static unsigned long LastMarioTimer = 0xFFFFFFFFul;
    if(MarioTimer == LastMarioTimer) return;
    LastMarioTimer = MarioTimer;

    unsigned StatusWidth = VidW*columns;

    //LongVecType Hdr(StatusWidth*2);
    static LongVecType Hdr(512);

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

    // LEFT-size parts
    char Part1[64]; sprintf(Part1, fnpart(CurrentFileName));
    char Part2[64]; sprintf(Part2, "Row %-5u/%u Col %u",
        (unsigned) (Cur.y+1),
        (unsigned) EditLines.size(), // (unsigned) EditLines.capacity(),
        (unsigned) (Cur.x+1));

    // RIGHT-side parts
    char Part3[16]; sprintf(Part3, "%02d:%02d:%02d",
        tm->tm_hour,tm->tm_min,tm->tm_sec);
    char Part4[32]; sprintf(Part4, "%lu/%lu C", chars_file, chars_typed);
    static const char Part5[] = "-4.2øC"; // temperature degC degrees celsius

    // Because running CPUinfo() interferes with our PIT clock,
    // only run the CPU speed check maybe twice in a second.
    static double cpuspeed = 12345678.90123;
    static unsigned long last_check_when = 0;
    #ifdef __BORLANDC__
    unsigned long now_when = (*(unsigned long*)MK_FP(0x40,0x6C)) & ~7ul;
    #elif defined(__DJGPP__)
    unsigned long now_when = _farpeekl(_dos_ds, 0x46C) & ~7ul;
    #else
    unsigned long now_when = 0;
    #endif
    if(last_check_when != now_when)
    {
        cpuspeed = CPUinfo();
        FixMarioTimer();
        last_check_when = now_when;
        Cycles_Check();
    }
    char Part6[32];
    if(cpuspeed >= 1e9)
        sprintf(Part6, "%1d.%1d GHz",
            (int)(cpuspeed * 1e-9),
            (int)(cpuspeed * 1e-8) % 10
        );
    else
        sprintf(Part6, "%3d MHz", (int)(cpuspeed * 1e-6));

    const char* Parts[6] = {Part1,Part2,Part3,Part4,Part5,Part6};
    const char Prio[6]   = {10,    13,  7,    4,    0,    2    };
    enum { NumParts = sizeof(Parts) / sizeof(*Parts) };

    static unsigned left_parts  = 0;
    static unsigned right_parts = 0;
    static unsigned last_check_width = 0;
    if(last_check_width != StatusWidth)
    {
        // When video width changes, determine which parts we can fit on the screen
        unsigned columns_remaining = StatusWidth - 8;
        last_check_width = StatusWidth;
        unsigned lengths[NumParts];
        for(unsigned n=0; n<6; ++n) lengths[n] = strlen(Parts[n]);
        left_parts  = 1;
        right_parts = 0;
        unsigned best_length = 0, best_prio = 0;
        for(unsigned combination = 0; combination < (1 << NumParts); ++combination)
        {
            unsigned length = 0, prio = 0;
            for(unsigned n=0; n<NumParts; ++n)
                if(combination & (1 << n))
                {
                    if(length) ++length;
                    length += lengths[n];
                    prio   += Prio[n];
                }
            if(length > columns_remaining) continue;
            if((length+prio) > (best_length+best_prio))
            {
                best_length = length;
                best_prio   = prio;
                left_parts  = combination & 3;
                right_parts = combination & ~3;
            }
        }
    }

    char Buf1[256]; Buf1[0] = '\0';
    char Buf2[256]; Buf2[0] = '\0';

    { unsigned leftn=0, leftl=0;
      unsigned rightn=0, rightl=0;
      // Put all left-parts in Buf1, space separated
      // Put all right-parts in Buf2, space separated
      for(unsigned n=0; n<NumParts; ++n)
      {
        unsigned bit = 1 << n;
        if(left_parts & bit)  { if(leftn++) Buf1[leftl++] = ' ';   leftl += sprintf(Buf1+leftl, "%s", Parts[n]); }
        if(right_parts & bit) { if(rightn++) Buf2[rightl++] = ' '; rightl += sprintf(Buf2+rightl, "%s", Parts[n]); }
      }
    }

    unsigned x1a = 7;
    unsigned x2a = StatusWidth-1 - strlen(Buf2);
    if(x2a & 0x8000) x2a = x1a + strlen(Buf1) + 1;
    unsigned x1b = x1a + strlen(Buf1);
    unsigned x2b = x2a + strlen(Buf2);

    {slide1.SetWidth(StatusWidth);
    for(unsigned x=0; x<StatusWidth; ++x)
    {
        unsigned char ch, c1, c2; slide1.Get(x, ch,c1,c2);

        if(C64palette) { c1=7; c2=0; ch = 0x20; }
        unsigned char c = 0x20;

        if(x == 0 && WaitingCtrl)
            c = '^';
        else if(x == 1 && WaitingCtrl)
            c = WaitingCtrl;
        else if(x == 3 && InsertMode)
            c = 'I';
        else if(x == 4 && UnsavedChanges)
            c = '*';
        else if(x >= x1a && x < x1b && Buf1[x-x1a] != ' ')
            c = (unsigned char)Buf1[x-x1a];
        else if(x >= x2a && x < x2b && Buf2[x-x2a] != ' ')
            c = (unsigned char)Buf2[x-x2a];

        if(c != 0x20)
        {
            ch = c; c2 = 0;
            //if(c1 == c2) c2 = 7;
        }
        /*if(!C64palette && c != 0x20)
            switch(c2)
            {
                case 8: c1 = 7; break;
                case 0: c1 = 8; break;
            }*/

        unsigned short colorlo = ch | 0x8000u | (c2 << 8u);
        unsigned short colorhi = c1 | 0x8000u | ((c2 & 0x80u) << 7u);

        if(FatMode)
            { Hdr[x+x] = colorlo; Hdr[x+x+1] = colorlo|0x80; }
        else
            Hdr[x] = colorlo | (((unsigned long)colorhi) << 16u);
    }}
    if(StatusLine[0])
        {slide2.SetWidth(StatusWidth);
        for(unsigned p=0,x=0; x<StatusWidth; ++x)
        {
            unsigned char ch, c1, c2; slide2.Get(x, ch,c1,c2);
            if(C64palette) { c1=7; c2=0; ch = 0x20; }

            unsigned char c = StatusLine[p]; if(!c) c = 0x20;
            if(c != 0x20)
            {
                ch = c; c2 = 0;
                //if(c1 == c2) c2 = 7;
            }
            /*if(!C64palette && c != 0x20)
                switch(c2)
                {
                    case 8: c1 = 7; break;
                    case 0: c1 = 8; break;
                }*/

            unsigned short colorlo = ch | 0x8000u | (c2 << 8u);
            unsigned short colorhi = c1 | 0x8000u | ((c2 & 0x80u) << 7u);

            if(FatMode)
                { Stat[x+x] = colorlo; Stat[x+x+1] = colorlo|0x80; }
            else
            {
                Stat[x] = colorlo;
                Stat[x+(DOSBOX_HICOLOR_OFFSET/2)] = colorhi;
            }
            if(StatusLine[p]) ++p;
        }}
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
    static LongVecType EmptyLine;

    unsigned winh = VidH - 1;
    if(StatusLine[0]) --winh;

    VisSoftCursor(-1);

    for(unsigned y=0; y<winh; ++y)
    {
        unsigned short* Tgt = GetVidMem(0, y+1);

        unsigned ly = Win.y + y;

        LongVecType* line = &EmptyLine;
        if(ly < EditLines.size()) line = &EditLines[ly];

        unsigned lw = line->size(), lx=0, x=Win.x, xl=x + VidW;
        unsigned trail = 0x0720;
        for(unsigned l=0; l<lw; ++l)
        {
            unsigned long attr = (*line)[l];
            if( (attr & 0xFF) == '\n' ) break;
            ++lx;
            if(lx > x)
            {
                if( ((ly == BlockBegin.y && lx-1 >= BlockBegin.x)
                  || ly > BlockBegin.y)
                &&  ((ly == BlockEnd.y && lx-1 < BlockEnd.x)
                  || ly < BlockEnd.y) )
                {
                    if((attr & 0x80008000ul) == 0x80008000ul)
                    {
                        unsigned char bg = (attr >> 16) & 0xFF;
                        unsigned char fg = ((attr >> 8) & 0x7F) | ((attr >> 23) & 0x80);
                        attr &= 0xBF0080FFul;
                        attr |= ((unsigned long)fg) << 16;
                        attr |= ((unsigned long)bg) << 8;
                        if(bg & 0x80) attr |= 0x40000000ul;
                    }
                    else
                    {
                        attr = (attr & 0xFF0000FFul)
                             | ((attr >> 4u) & 0xF00u)
                             | ((attr << 4u) & 0xF000u);
                    }
                }
                if(DispUcase && islower(attr & 0xFF))
                    attr &= ~0x20ul;

                do {
                    Tgt[(DOSBOX_HICOLOR_OFFSET/2)] = (attr >> 16);
                    *Tgt++ = attr;
                    if(FatMode) *Tgt++ = attr | 0x80;
                } while(lx > ++x);
                if(x >= xl) break;
            }
        }
        while(x++ < xl)
        {
            Tgt[(DOSBOX_HICOLOR_OFFSET/2)] = 0;
            *Tgt++ = trail;
            if(FatMode) *Tgt++ = trail | 0x80;
        }
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
        if(StatusLine[0] && Cur.y >= Win.y+VidH-2) Win.y += 2;
    }

    Cycles_Adjust(1);

    if(!kbhit())
    {
        while(Cur.x < Win.x) Win.x -= 8;
        while(Cur.x >= Win.x + VidW) Win.x += 8;
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
        #if defined(__BORLANDC__) || defined(__DJGPP__)
            if(SyntaxCheckingNeeded == SyntaxChecking_IsPerfect
            || SyntaxCheckingNeeded != SyntaxChecking_DoingFull)
            {
                if(SyntaxCheckingNeeded == SyntaxChecking_IsPerfect)
                    Cycles_Adjust(-1);
              #ifdef __BORLANDC__
                _asm { hlt }
              #else
                //__dpmi_yield();
                __asm__ volatile("hlt");
                // dpmi_yield is not used, because it's not implemented in dosbox
                // Instead, we issue "hlt" and patch DOSBox to not produce an exception
                /* HUGE WARNING: THIS *REQUIRES* A PATCHED DOSBOX,
                 * UNPATCHED DOSBOXES WILL TRIGGER AN EXCEPTION HERE */
              #endif
            }
        #endif
        } while(!kbhit());
    }
}

struct UndoEvent
{
    unsigned x, y;
    unsigned n_delete;
    LongVecType insert_chars;
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
    const LongVecType& insert_chars,
    char DoingUndo = 0)
{
    unsigned eol_x = EditLines[y].size();
    if(eol_x > 0 && (EditLines[y].back() & 0xFF) == '\n') --eol_x;
    if(x > eol_x) x = eol_x;

    UndoEvent event;
    event.x = x;
    event.y = y;
    event.n_delete = 0;

    chars_file += insert_chars.size();

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

        chars_file += event.insert_chars.size();
        event.insert_chars.insert(
            event.insert_chars.end(),
            EditLines[y].begin() + x,
            EditLines[y].begin() + x + n_delete);
        chars_file -= event.insert_chars.size();

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
            LongVecType nlvec(1, '\n' | UnknownColor);
            LongPtrVecType new_lines( insert_newline_count, nlvec );
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
        LongVecType indentbuf(offset, UnknownColor | 0x20);
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
    else if(int(min_indent) >= -offset)
    {
        unsigned outdent = -offset;
        LongVecType empty;
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

void GetBlock(LongVecType& block)
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
    unsigned char wdblset[5] = { (unsigned char)dblw, (unsigned char)dblw, (unsigned char)!dblw, (unsigned char)dblw, (unsigned char)!dblw };
    unsigned char hdblset[5] = { (unsigned char)dblh, (unsigned char)dblh, (unsigned char)dblh, (unsigned char)!dblh, (unsigned char)!dblh };
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
                  if(FatMode) { p[2*a]=c; p[2*a+1]=c|0x80; } else { p[a] = c; p[a+(DOSBOX_HICOLOR_OFFSET/2)] = 0; } 
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
            p[m+(DOSBOX_HICOLOR_OFFSET/2)] = 0;
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
            { if(sel_y < int(noptions-1)) sel_y=noptions-1; else sel_x=4; }
        else if(c == 0)
            switch(getch())
            {
                case 'H': up:
                    if(sel_y < 0) sel_y = noptions;
                    --sel_y;
                    break;
                case 'P': dn:
                    if(++sel_y >= int(noptions)) sel_y = -1;
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
    if(sel_y == -1) return 0;
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
    if(Win.y > Cur.y || Win.y+DimY-1 <= Cur.y)
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

static void k_home(void)
{
    unsigned indent = 0;
    while(indent < EditLines[Cur.y].size()
       && (EditLines[Cur.y][indent] & 0xFF) == ' ') ++indent;
    // indent = number of spaces in the beginning of the line
    Cur.x = (Cur.x == indent ? 0 : indent);
}
static void k_end(void)
{
    Cur.x = EditLines[Cur.y].size();
    if(Cur.x > 0 && (EditLines[Cur.y].back() & 0xFF) == '\n')
        --Cur.x; /* past LF */
}
static void k_left(void)
{
    if(Cur.x > EditLines[Cur.y].size())
        Cur.x = EditLines[Cur.y].size()-1;
    else if(Cur.x > 0) --Cur.x;
    else if(Cur.y > 0) { --Cur.y; k_end(); }
}
static void k_right(void)
{
    if(Cur.x+1 < EditLines[Cur.y].size())
        ++Cur.x;
    else if(Cur.y+1 < EditLines.size())
        { Cur.x = 0; ++Cur.y; }
}
static void k_ctrlleft(void)
{
    // Algorithm adapted from Joe 3.7
    #define at_line_end() (Cur.y >= EditLines.size() || Cur.x >= EditLines[Cur.y].size())
    #define at_begin() (Cur.x==0 && Cur.y==0)
    #define cur_ch     (EditLines[Cur.y][Cur.x]&0xFF)

    // First go one left.
    k_left();
    // Skip possible space
    while(!at_begin() && (at_line_end() || isspace(cur_ch) || ispunct_(cur_ch))) { k_left(); }
    // Then skip to the beginning of the current word
    while(!at_begin() && !at_line_end() && isalnum_(cur_ch)) { k_left(); }
    // Then undo the last k_left, unless we're at the beginning of the file
    if(!at_begin()) k_right();

    #undef cur_ch
    #undef at_begin
    #undef at_line_end
}
static void k_ctrlright(void)
{
    // Algorithm adapted from Joe 3.7
    #define at_line_end() (Cur.y >= EditLines.size() || Cur.x >= EditLines[Cur.y].size())
    #define at_end()      (Cur.y >= EditLines.size() || ((Cur.y+1) == EditLines.size() && Cur.x == EditLines[Cur.y].size()))
    #define cur_ch        (EditLines[Cur.y][Cur.x]&0xFF)
    if(at_end()) return;

    // First skip possible space
    while(!at_end() && (at_line_end() || isspace(cur_ch) || ispunct_(cur_ch))) { k_right(); }
    // Then skip to the end of the current word
    while(!at_line_end() && isalnum_(cur_ch)) { k_right(); }

    #undef cur_ch
    #undef at_end
    #undef at_line_end
}

int main(int argc, char**argv)
{
  #if 1
    // Set mode (for dosbox recording)
    //VgaSetCustomMode(80,50,16,1,0,0, 2); columns = 2;
  #endif

#ifdef __DJGPP__
    __djgpp_nearptr_enable();
#endif

#if defined(__BORLANDC__) || defined(__DJGPP__)
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

#if defined(__BORLANDC__) || defined(__DJGPP__)
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
#elif defined(__DJGPP__)
        int shift = 3 & _farpeekb(_dos_ds, 0x417);
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
        unsigned /*DimX = VidW,*/ DimY = VidH-1;
        char WasAppend = 0;
        chars_typed += 1;
        switch(c)
        {
            case CTRL('V'): // ctrl-V
            {
            pgdn:;
                unsigned offset = Cur.y-Win.y;
                Cur.y += DimY;
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
                if(Cur.y > DimY) Cur.y -= DimY; else Cur.y = 0;
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
                StatusLine[0] = 0;
                break;
            case CTRL('Z'):
                if(Win.y+1/*+DimY*/ < EditLines.size()) ++Win.y;
                if(Cur.y < Win.y) Cur.y = Win.y;
                StatusLine[0] = 0;
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
                        LongVecType block, empty;
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
                        LongVecType block;
                        GetBlock(block);
                        unsigned x = Cur.x, y = Cur.y;
                        PerformEdit(Cur.x,Cur.y, InsertMode?0u:block.size(), block);
                        BlockBegin.x = x; BlockBegin.y = y;
                        BlockEnd.x = Cur.x; BlockEnd.y = Cur.y;
                        break;
                    }
                    case 'y': case 'Y': case CTRL('Y'): // delete block
                    {
                        LongVecType block, empty;
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
                        LongVecType txtbuf(1, 0x0700 | (c & 0xFF));
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
                        if(Cur.y >= Win.y+DimY) Win.y = Cur.y - DimY+1;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 0x47: // home
                    {
                    home:
                        k_home();
                        Win.x = 0;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 0x4F: // end
                    end:
                        k_end();
                        Win.x = 0;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 'K': // left
                    {
                    lt_key:
                        k_left();
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 'M': // right
                    {
                    rt_key:
                        k_right();
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 0x73: // ctrl-left (go left on word boundary)
                    {
                        k_ctrlleft();
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    }
                    case 0x74: // ctrl-right (go right on word boundary)
                    {
                        k_ctrlright();
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
                        if(Cur.y >= Win.y+DimY) Win.y = Cur.y - DimY+1;
                        goto end;
                    case 0x77: // ctrl-home = goto beginning of window (vertically)
                        Cur.y = Win.y;
                        if(shift && ENABLE_DRAG) dragalong = 1;
                        break;
                    case 0x75: // ctrl-end = goto end of window (vertically)
                        Cur.y = Win.y + VidH-1;
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
                        LongVecType empty;
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
                    case 0x3F: use9bit = !use9bit; goto newmode; // F5
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
                LongVecType empty;
                PerformEdit(Cur.x,Cur.y, EditLines[Cur.y].size(), empty);
                break;
            }
            case CTRL('H'): // backspace = left + delete
            {
                LongVecType empty;
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
                LongVecType txtbuf(nspaces, 0x0720);
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
                LongVecType txtbuf(nspaces + 1, 0x0720);
                txtbuf[0] = 0x070A; // newline
                PerformEdit(Cur.x,Cur.y, InsertMode?0u:1u, txtbuf);
                //Win.x = 0;
                WasAppend = 1;
                break;
            }
            default:
            {
                LongVecType txtbuf(1, 0x0700 | c);
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
    Cur.x = 0; Cur.y = Win.y + VidH-2; InsertMode = 1;
    if(FatMode || C64palette)
    {
        FatMode=0;
        C64palette=0;
        VgaSetCustomMode(VidW,VidH, VidCellHeight,
                         use9bit, dblw, dblh, columns);
    }
    VisSetCursor();
#if defined(__BORLANDC__) || defined(__DJGPP__)
    DeInstallMario();
#endif
    exit(0);
    return 0;
}
