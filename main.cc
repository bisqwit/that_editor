/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "langdefs.hh"
#include "kbhit.hh"

#include "vga.hh"
#include "chartype.hh"
#include "jsf.hh"

#include "cpu.h"

#ifdef __BORLANDC__
# include <process.h> // For Cycles adjust on DOSBOX
# include <dos.h> // MK_FP, getpsp, inportb
#endif
#ifdef __DJGPP__
# include <dos.h>
# include <dpmi.h>
# include <go32.h>
# include <sys/farptr.h>
#endif

#define CTRL(c) ((c) & 0x1F)

static const bool ENABLE_DRAG = false;

static unsigned long chars_file  = 0;
static unsigned long chars_typed = 0;

static bool use9bit=false, dblw=false, dblh=false;

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

char StatusLine[256] = // WARNING: Not range-checked
"Ad-hoc programming editor - (C) 2011-03-08 Joel Yliluoma";

EditorLineVecType EditLines;

struct Anchor
{
    size_t x, y;
    Anchor() : x(0), y(0) { }
};

enum { MaxSavedCursors = 4, NumCursors = MaxSavedCursors + 2 };

Anchor Win, SavedCursors[NumCursors];

bool          InsertMode   =true;
char          WaitingCtrl  =0;
unsigned char CurrentCursor=0;
unsigned char TabSize      =4;

#define Cur        SavedCursors[CurrentCursor]
#define BlockBegin SavedCursors[MaxSavedCursors  ]
#define BlockEnd   SavedCursors[MaxSavedCursors+1]

bool  UnsavedChanges  = false;
char* CurrentFileName = nullptr;

static void FileLoad(const char* fn)
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
    EditorCharVecType editline;
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
                    editline.resize(nextstop, MakeUnknownColor(' '));
                    /*while(editline.size() < nextstop)
                        editline.push_back( MakeUnknownColor(' ') );*/
                }
                else
                    editline.push_back( MakeUnknownColor(c) );

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
    UnsavedChanges = false;

    chars_file = 0;
    for(size_t a=0; a<EditLines.size(); ++a)
        chars_file += EditLines[a].size();
}
static void FileNew()
{
    Win = Cur = Anchor();
    EditLines.clear();
    EditorCharVecType emptyline;
    emptyline.push_back(MakeUnknownColor('\n'));
    EditLines.push_back(emptyline);
    EditLines.push_back(emptyline);
    EditLines.push_back(emptyline);

    if(CurrentFileName) free(CurrentFileName);
    CurrentFileName = 0;
    chars_file = 3; // Three newlines
}
struct ApplyEngine: public JSF::Applier
{
    bool finished;
    unsigned nlinestotal, nlines;
    size_t x,y, begin_line;
    unsigned pending_recolor_distance, pending_recolor;
    EditorCharType pending_attr;
    ApplyEngine()
        { Reset(0); }
    void Reset(size_t line)
        { x=0; y=begin_line=line; finished=false; nlinestotal=nlines=0;
          pending_recolor=0;
          pending_attr   =0;
        }
    virtual cdecl int Get(void)
    {
        if(y >= EditLines.size() || EditLines[y].empty())
        {
            finished = true;
            FlushColor();
            return -1;
        }
        int ret = ExtractCharCode(EditLines[y][x]);
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
    virtual cdecl void Recolor(register unsigned distance, register unsigned n, register EditorCharType attr)
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
        pending_attr             = attr;
    }
private:
    void FlushColor()
    {
        register unsigned dist       = pending_recolor_distance;
        register unsigned n          = pending_recolor;
        register EditorCharType attr = pending_attr;
        if(n > 0)
        {
            //fprintf(stdout, "Recolors %u as %02X\n", n, attr);
            size_t px=x, py=y;
            EditorCharVecType* line = &EditLines[py];
            for(n += dist; n > 0; --n)
            {
                if(px == 0) { if(!py) break; line = &EditLines[--py]; px = line->size()-1; }
                else --px;
                if(dist > 0)
                    --dist;
                else
                {
                    EditorCharType& w = (*line)[px];
                    w = ::Recolor(w, attr);
                }
            }
        }
        pending_recolor          = 0;
        pending_recolor_distance = 0;
    }
};


#include "mario.hh"

unsigned CursorCounter = 0;

/* SoftCursor is only used in the C64 simulation mode. */
static void VisSoftCursor(int mode)
{
    if(!C64palette) return; // Don't do anything if C64palette is not activated.

    /* Modes:
     *   0 = blink ticker (100 Hz)
     *   1 = screen contents have changed
     *  -1 = request undo the cursor at current location
     */
    static unsigned short* cursor_location = nullptr;
    static unsigned evacuation = 0;

    // *READ* hardware cursor position from screen */
    unsigned char cux=0, cuy=0;
#ifdef __BORLANDC__
    _asm { mov ah, 3; mov bh, 0; int 0x10; mov cux, dl; mov cuy, dh; xchg cx,cx }
#elif defined(__DJGPP__)
    { REGS r{}; r.h.ah = 3; r.h.bh = 0; int86(0x10, &r, &r); cux = r.h.dl; cuy = r.h.dh; }
#endif
    // Get the corresponding location in the video memory
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
            *cursor_location = ((evacuation&0xF00) << 4) | evacuation;
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
    outportw(0x3D4, 0x0E + (addr&0xFF00));
    outportw(0x3D4, 0x0F + ((addr&0xFF)<<8));
    // Set cursor shape: lines-4 to lines-3, or 6 to 7
    outportw(0x3D4, 0x0A + ((VidCellHeight>8 ? VidCellHeight-4 : 6) << 8));
    outportw(0x3D4, 0x0B + ((VidCellHeight>8 ? VidCellHeight-3 : 7) << 8));
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

#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
static const unsigned char  slide1_colors[21] = {6,73,109,248,7,7,7,7,7,248,109,73,6,6,6,36,35,2,2,28,22};
static const unsigned short slide1_positions[21] = {0u,1401u,3711u,6302u,7072u,8192u,16384u,24576u,32768u,33889u,34659u,37250u,39560u,40960u,49152u,50903u,53634u,55944u,57344u,59937u,63981u};
static const unsigned char  slide2_colors[35] = {248,7,249,250,251,252,188,253,254,255,15,230,229,228,227,11,227,185,186,185,179,143,142,136,100,94,58,239,238,8,236,235,234,233,0};
static const unsigned short slide2_positions[35] = {0u,440u,1247u,2126u,3006u,3886u,4839u,5938u,6965u,8064u,9750u,12590u,15573u,18029u,19784u,21100u,24890u,27163u,30262u,35051u,35694u,38054u,40431u,41156u,46212u,46523u,50413u,52303u,53249u,54194u,56294u,58815u,61335u,63856u,64696u};
#endif
#ifdef ATTRIBUTE_CODES_IN_VGA_ORDER
static const unsigned char  slide1_colors[12] = {3,7,7,7,7,7,3,3,3,2,2,2};
static const unsigned short slide1_positions[12] = {0u,4132u,8192u,16384u,24576u,32768u,36829u,40960u,49152u,52583u,53876u,57344u};
static const unsigned char  slide2_colors[11] = {7,15,15,14,14,14,6,6,8,8,0};
static const unsigned short slide2_positions[11] = {0u,5541u,10923u,16363u,21846u,32768u,38151u,43691u,49153u,54614u,59655u};
#endif

static ColorSlideCache slide1(slide1_colors, slide1_positions, sizeof(slide1_colors));
static ColorSlideCache slide2(slide2_colors, slide2_positions, sizeof(slide2_colors));

/* Renders status-bar on screen, if non-empty. */
static void VisRenderStatusLine()
{
    // Only render Statusline if non-empty
    if(!StatusLine[0]) return;

    unsigned short* Stat = GetVidMem(0, (VidH-1) / columns, 1);

    const unsigned StatusWidth = VidW*columns;

    slide2.SetWidth(StatusWidth);
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

        VidmemPutEditorChar(ComposeEditorChar(ch, c2, c1), Stat);
        if(StatusLine[p]) ++p;
    }
}

static const char* StatusGetCPUspeed()
{
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
    static char Part6[18]; // 11+2+4+nul
    if(last_check_when != now_when)
    {
        cpuspeed = CPUinfo();
        FixMarioTimer();
        last_check_when = now_when;
        Cycles_Check();
        if(cpuspeed >= 1e9)
            sprintf(Part6, "%1d.%1d GHz",
                (int)(cpuspeed * 1e-9),
                (int)(cpuspeed * 1e-8) % 10
            );
        else
            sprintf(Part6, "%3d MHz", (int)(cpuspeed * 1e-6));
    }
    return Part6;
}

static const char* StatusGetClock()
{
    static time_t last_t = 0;
    time_t t = time(0);
    static char Part3[12]; // 5+1+2+1+2+nul
    if(!last_t || t != last_t)
    {
        last_t = t;
        struct tm* tm = localtime(&t);
        /*
        static unsigned long begintime = *(unsigned long*)MK_FP(0x40,0x6C);
        unsigned long nowtime          = *(unsigned long*)MK_FP(0x40,0x6C);
        unsigned long time = (47*60+11) - 759 + (nowtime-begintime)*(65536.0/0x1234DC);
        tm->tm_hour = 18;
        tm->tm_min  = time/60;
        tm->tm_sec  = time%60;
        */
        sprintf(Part3, "%02d:%02d:%02d", tm->tm_hour,tm->tm_min,tm->tm_sec);
    }
    return Part3;
}


/* VisRenderTitleAndStatus: Renders the title bar and the status bar.
 *                  Status bar is only rendered if non-empty.
 */
static void VisRenderTitleAndStatus()
{
    // Only re-render status once per frame
    static unsigned long LastMarioTimer = 0xFFFFFFFFul;
    if(MarioTimer == LastMarioTimer) return;
    LastMarioTimer = MarioTimer;

    unsigned StatusWidth = VidW*columns;

    // LEFT-size parts
    const char* Part1 = fnpart(CurrentFileName);
    static char Part2[44]; sprintf(Part2, "Row %-5u/%u Col %u", // 4+11+1+11+5+11+nul
        (unsigned) (Cur.y+1),
        (unsigned) EditLines.size(), // (unsigned) EditLines.capacity(),
        (unsigned) (Cur.x+1));

    // RIGHT-side parts
    const char* Part3 = StatusGetClock();
    static char Part4[26]; sprintf(Part4, "%lu/%lu C", chars_file, chars_typed); //11+1+11+2+nul
    static const char Part5[] = "-4.2øC"; // temperature degC degrees celsius

    const char* Part6 = StatusGetCPUspeed();

    const unsigned NumParts = 6;
    static const char* const Parts[6] = {Part1,Part2,Part3,Part4,Part5,Part6};
    static const char Prio[6]  = {10,    13,  7,    4,    0,    2    };

    unsigned Lengths[NumParts];
    {for(unsigned n=0; n<NumParts; ++n) Lengths[n] = strlen(Parts[n]);}

    // Create a plan which parts are rendered on the left side of the title bar,
    // and which parts are rendered on the right side. Cache this plan.
    // The plan is only updated whenever the screen width changes.
    static unsigned left_parts  = 0, right_parts = 0, last_check_width = 0;
    if(last_check_width != StatusWidth)
    {
        // When video width changes, determine which parts we can fit on the screen
        unsigned columns_remaining = StatusWidth - 8;
        last_check_width = StatusWidth;

        // Check which combination of columns produces the best fit.
        // Try all 2^6 = 64 combinations.
        unsigned allowed_parts = 1, best_length = 0, best_prio = 0;
        for(unsigned combination = 0; combination < (1 << NumParts); ++combination)
        {
            unsigned length = 0, prio = 0;
            for(unsigned n=0; n<NumParts; ++n)
                if(combination & (1 << n))
                {
                    if(length) ++length;
                    length += Lengths[n];
                    prio   += Prio[n];
                }
            if(length > columns_remaining) continue;
            if((length+prio) > (best_length+best_prio))
            {
                best_length = length;
                best_prio   = prio;
                allowed_parts = combination;
            }
        }
        // Only the first two parts go to the left, anything else goes on right
        left_parts  = allowed_parts & 3;
        right_parts = allowed_parts & ~3;
    }

    unsigned xbegin[NumParts], xend[NumParts];
    memset(xbegin,0,sizeof(xbegin)); memset(xend,0,sizeof(xend));

    unsigned leftn=6;  // Calculate starting positions for left-side parts
    unsigned rightn=0; // Calculate relative starting positions for right-side parts
    {for(unsigned n=0; n<NumParts; ++n)
    {
        unsigned bit = 1 << n;
        if(left_parts & bit)  { if(leftn) { ++leftn; }   xbegin[n] = leftn;  xend[n] = (leftn  += Lengths[n]); }
        if(right_parts & bit) { if(rightn) { ++rightn; } xbegin[n] = rightn; xend[n] = (rightn += Lengths[n]); }
    }}
    // Adjust the right-side starting positions
    int      right_start = StatusWidth - 1 - rightn;
    int      left_end    = leftn+1;
    if(right_start < left_end) right_start = left_end;
    {for(unsigned n=0; n<NumParts; ++n)
        if(right_parts & (1u << n)) { xbegin[n] += right_start; xend[n] += right_start; }
    }

    // Now render the actual status line into Hdr[] with colors.
    static EditorCharVecType Hdr; Hdr.resize(FatMode ? StatusWidth*2 : StatusWidth);

    slide1.SetWidth(StatusWidth);
    for(unsigned x=0; x<StatusWidth; ++x)
    {
        unsigned char ch, c1, c2; slide1.Get(x, ch,c1,c2);

        if(C64palette) { c1=7; c2=0; ch = 0x20; }

        // Identify the character at this position
        unsigned char c = 0x20;
        if(x == 0 && WaitingCtrl)                            c = '^';
        else if(x == 1 && WaitingCtrl)                       c = WaitingCtrl;
        else if(x == 3 && InsertMode)                        c = 'I';
        else if(x == 4 && UnsavedChanges)                    c = '*';
        else for(unsigned n=0; n<NumParts; ++n)
            if(x < xend[n] && x >= xbegin[n])
            {
                c = (unsigned char) Parts[n][x - xbegin[n]];
                break;
            }

        // Convert the character into rendered symbol
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

        Hdr[x] = ComposeEditorChar(ch, c2, c1);
    }
    // Then translate the Hdr[] buffer on screen using MarioTranslate.
    MarioTranslate(&Hdr[0], GetVidMem(0,0,1), StatusWidth);

    VisRenderStatusLine();
}

/* VisRender: Render whole screen except the status line */
static void VisRender()
{
    static EditorCharVecType EmptyLine; // Dummy vector representing an empty line

    // Hide soft-cursor
    VisSoftCursor(-1);

    unsigned winh = VidH - 1;
    if(StatusLine[0]) --winh;
    for(unsigned y=0; y<winh; ++y)
    {
        unsigned short* Tgt = GetVidMem(0, y+1);

        unsigned ly = Win.y + y;

        EditorCharVecType* line = &EmptyLine;
        if(ly < EditLines.size()) line = &EditLines[ly];

        unsigned lw = line->size(), lx=0, x=Win.x, xl=x + VidW;
        EditorCharType trail = MakeDefaultColor(' ');
        for(unsigned l=0; l<lw; ++l)
        {
            EditorCharType attr = (*line)[l];
            if(ExtractCharCode(attr) == '\n') break;
            ++lx;
            if(lx > x)
            {
                if( ((ly == BlockBegin.y && lx-1 >= BlockBegin.x)
                  || ly > BlockBegin.y)
                &&  ((ly == BlockEnd.y && lx-1 < BlockEnd.x)
                  || ly < BlockEnd.y) )
                {
                    attr = InvertColor(attr);
                }
                if(DispUcase && islower(ExtractCharCode(attr)))
                    attr &= ~0x20ul;

                do VidmemPutEditorChar(attr, Tgt); while(lx > ++x);
                if(x >= xl) break;
            }
        }
        while(x++ < xl) VidmemPutEditorChar(trail, Tgt);
    }

    // Redraw soft-cursor
    VisSoftCursor(1);
}

unsigned wx = ~0u, wy = ~0u, cx = ~0u, cy = ~0u;

enum SyntaxCheckingType
{
    SyntaxChecking_IsPerfect = 0,
    SyntaxChecking_DidEdits = 1,
    SyntaxChecking_Interrupted = 2,
    SyntaxChecking_DoingFull = 3
} SyntaxCheckingNeeded = SyntaxChecking_DoingFull;

JSF             Syntax;
JSF::ApplyState SyntaxCheckingState;
ApplyEngine     SyntaxCheckingApplier;

/* TODO: In syntax checking: If the syntax checker ever reaches the current editing line,
 *                           make a save in the beginning of the line and use that for resuming
 *                           instead of backtracking to the context
 */

// How many lines to backtrack
#define SyntaxChecking_ContextOffset 50

static void WaitInput(bool may_redraw = true)
{
    if(may_redraw)
    {
        // If the cursor position has changed from last update,
        // hide the software cursor and replace the hardware cursor
        if(cx != Cur.x || cy != Cur.y) { cx=Cur.x; cy=Cur.y; VisSoftCursor(-1); VisSetCursor(); }
        // If statusline is visible and cursor coincides with statusline, scroll screen down
        if(StatusLine[0] && Cur.y >= Win.y+VidH-2) Win.y += 2;
    }

    // There is work to do, so request more CPU speed
    Cycles_Adjust(1);

    // Quickly skip doing anything if no key has been presed
    if(kbhit()) return;

    // Adjust window position horizontally making sure cursor is on screen
    while(Cur.x < Win.x)         Win.x -= 8;
    while(Cur.x >= Win.x + VidW) Win.x += 8;

    bool needs_redraw = false;

    // If the window position has changed from last update,
    // hide the software cursor and replace the hardware cursor
    if(may_redraw && (wx != Win.x || wy != Win.y))
    {
        VisSoftCursor(-1);
        VisSetCursor();
        // Delay the screen refreshing though,
        // so we only do it once
        needs_redraw = true;
    }

    while(!kbhit())
    {
        if(may_redraw)
        {
            if(SyntaxCheckingNeeded != SyntaxChecking_IsPerfect)
            {
                bool horrible_sight =
                    ExtractColor(VidmemReadEditorChar(VidMem+VidW*2))          == MakeUnknownColor('\0')
                ||  ExtractColor(VidmemReadEditorChar(VidMem+VidW*(VidH*3/4))) == MakeUnknownColor('\0');
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
                // Apply syntax coloring. Will continue applying colors until
                // either a key is pressed, or the checking finishes.
                Syntax.Apply(SyntaxCheckingState, SyntaxCheckingApplier);

                // Update the need-syntax-checking state
                SyntaxCheckingNeeded =
                    !SyntaxCheckingApplier.finished
                    ? SyntaxChecking_Interrupted
                    : (SyntaxCheckingApplier.begin_line == 0)
                        ? SyntaxChecking_IsPerfect
                        : SyntaxChecking_DoingFull;

                // Something was changed, so refresh screen now
                needs_redraw = true;
            }
            // In any case, refresh screen if _something_ changed
            if(needs_redraw)
                { wx=Win.x; wy=Win.y; VisRender(); needs_redraw = false; }
        }
        VisRenderTitleAndStatus();
        VisSoftCursor(0);

        if(SyntaxCheckingNeeded == SyntaxChecking_IsPerfect
        || SyntaxCheckingNeeded != SyntaxChecking_DoingFull)
        {
            if(SyntaxCheckingNeeded == SyntaxChecking_IsPerfect)
                Cycles_Adjust(-1);
          #ifdef __BORLANDC__
            _asm { hlt }
          #elif defined(__DJGPP__)
            //__dpmi_yield();
            __asm__ volatile("hlt");
            // dpmi_yield is not used, because it's not implemented in dosbox
            // Instead, we issue "hlt" and patch DOSBox to not produce an exception
            /* HUGE WARNING: THIS *REQUIRES* A PATCHED DOSBOX,
             * UNPATCHED DOSBOXES WILL TRIGGER AN EXCEPTION HERE */
          #endif
        }
    }
}

struct UndoEvent
{
    unsigned x, y;
    unsigned n_delete;
    EditorCharVecType insert_chars;
};
const unsigned   MaxUndo = 256;
UndoEvent UndoQueue[MaxUndo];
UndoEvent RedoQueue[MaxUndo];
unsigned  UndoHead = 0, RedoHead = 0;
unsigned  UndoTail = 0, RedoTail = 0;
bool      UndoAppendOk = false;
static void AddUndo(const UndoEvent& event)
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
static void AddRedo(const UndoEvent& event)
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

enum UndoType
{
    DoingUndo_Not,
    DoingUndo_Undo,
    DoingUndo_Redo
};
static void PerformEdit(
    unsigned x, unsigned y,
    unsigned n_delete,
    const EditorCharVecType& insert_chars,
    UndoType DoingUndo = DoingUndo_Not)
{
    unsigned eol_x = EditLines[y].size();
    if(eol_x > 0 && ExtractCharCode(EditLines[y].back()) == '\n') --eol_x;
    if(x > eol_x) x = eol_x;

    UndoEvent event;
    event.x = x;
    event.y = y;
    event.n_delete = 0;

    chars_file += insert_chars.size();

    if(DoingUndo)
    {
        int s = sprintf(StatusLine,"Edit%u @%u,%u: Delete %u, insert '",
                                   int(UndoAppendOk), x,y,n_delete);
        for(unsigned b=insert_chars.size(), a=0; a<b && s<252; ++a)
        {
            char c = ExtractCharCode(insert_chars[a]);
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
            if(ExtractCharCode(insert_chars[p]) == '\n')
                ++insert_newline_count; }

        if(insert_newline_count > 0)
        {
            EditorCharVecType nlvec(1, MakeUnknownColor('\n'));
            EditorLineVecType new_lines( insert_newline_count, nlvec );
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
            if(ExtractCharCode(insert_chars[insert_beginpos]) == '\n')
                { x = 0; ++y; ++insert_beginpos; }
            else
            {
                unsigned p = insert_beginpos;
                while(p < insert_length && ExtractCharCode(insert_chars[p]) != '\n')
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
        case DoingUndo_Not: // normal edit
            RedoHead = RedoTail = 0; // reset redo
            AddUndo(event); // add undo
            break;
        case DoingUndo_Undo:
            AddRedo(event);
            break;
        case DoingUndo_Redo:
            AddUndo(event); // add undo, but don't reset redo
            break;
    }

    UnsavedChanges = true;
}

static void PerformEdit(unsigned n_delete, unsigned n_insert, const unsigned char* insert_source)
{
    EditorCharVecType txtbuf(n_insert);
    for(unsigned n=0; n<n_insert; ++n) txtbuf[n] = MakeDefaultColor(insert_source[n]);
    PerformEdit(Cur.x, Cur.y, n_delete, txtbuf);
}
static void PerformEdit(unsigned n_delete, unsigned n_insert1, unsigned char ch1, unsigned n_insert2=0, unsigned char ch2=0)
{
    EditorCharVecType txtbuf(n_insert1 + n_insert2);
    {for(unsigned n=0; n<n_insert1; ++n) txtbuf[n            ] = MakeDefaultColor(ch1);}
    {for(unsigned n=0; n<n_insert2; ++n) txtbuf[n + n_insert1] = MakeDefaultColor(ch2);}
    PerformEdit(Cur.x, Cur.y, n_delete, txtbuf);
}

static void TryUndo()
{
    unsigned UndoBufSize = (UndoHead + MaxUndo - UndoTail) % MaxUndo;
    if(UndoBufSize > 0)
    {
        UndoHead = (UndoHead + MaxUndo-1) % MaxUndo;
        UndoEvent event = UndoQueue[UndoHead]; // make copy
        PerformEdit(event.x, event.y, event.n_delete, event.insert_chars, DoingUndo_Undo);
    }
}
static void TryRedo()
{
    unsigned RedoBufSize = (RedoHead + MaxUndo - RedoTail) % MaxUndo;
    if(RedoBufSize > 0)
    {
        RedoHead = (RedoHead + MaxUndo-1) % MaxUndo;
        UndoEvent event = RedoQueue[RedoHead]; // make copy
        PerformEdit(event.x, event.y, event.n_delete, event.insert_chars, DoingUndo_Redo);
    }
}

static void BlockIndent(int offset)
{
    unsigned firsty = BlockBegin.y, lasty = BlockEnd.y;
    if(BlockEnd.x == 0) lasty -= 1;

    unsigned min_indent = 0x7FFF, max_indent = 0;
    for(unsigned y=firsty; y<=lasty; ++y)
    {
        unsigned indent = 0;
        while(indent < EditLines[y].size()
           && ExtractCharCode(EditLines[y][indent]) == ' ') ++indent;
        if(ExtractCharCode(EditLines[y][indent]) == '\n') continue;
        if(indent < min_indent) min_indent = indent;
        if(indent > max_indent) max_indent = indent;
    }
    if(offset > 0)
    {
        EditorCharVecType indentbuf(offset, MakeUnknownColor(' '));
        for(unsigned y=firsty; y<=lasty; ++y)
        {
            unsigned indent = 0;
            while(indent < EditLines[y].size()
                  && ExtractCharCode(EditLines[y][indent]) == ' ') ++indent;
            if(ExtractCharCode(EditLines[y][indent]) == '\n') continue;
            PerformEdit(0u,y, 0u, indentbuf);
        }
        if(BlockBegin.x > 0) BlockBegin.x += offset;
        if(BlockEnd.x   > 0) BlockEnd.x   += offset;
    }
    else if(int(min_indent) >= -offset)
    {
        unsigned outdent = -offset;
        EditorCharVecType empty;
        for(unsigned y=firsty; y<=lasty; ++y)
        {
            unsigned indent = 0;
            while(indent < EditLines[y].size()
               && ExtractCharCode(EditLines[y][indent]) == ' ') ++indent;
            if(ExtractCharCode(EditLines[y][indent]) == '\n') continue;
            if(indent < outdent) continue;
            PerformEdit(0u,y, outdent, empty);
        }
        if(BlockBegin.x >= outdent) BlockBegin.x -= outdent;
        if(BlockEnd.x   >= outdent) BlockEnd.x   -= outdent;
    }
    SyntaxCheckingNeeded = SyntaxChecking_DidEdits;
}

static void GetBlock(EditorCharVecType& block)
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

static inline void FindPair()
{
    int           PairDir  = 0;
    unsigned char PairChar = 0;
    unsigned char PairColor = EditLines[Cur.y][Cur.x] >> 8; // FIXME
    switch(ExtractCharCode(EditLines[Cur.y][Cur.x]))
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
            unsigned char c = ExtractCharCode(EditLines[testy][testx]);
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
            unsigned char c = ExtractCharCode(EditLines[testy][testx]);
            if(balance == 0 && c == PairChar) { Cur.x = testx; Cur.y = testy; return; }
            if(c == '{' || c == '[' || c == '(') ++balance;
            if(c == '}' || c == ']' || c == ')') --balance;
        }
}

static int SelectFont()
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
    bool wdblset[5] = { dblw, dblw, !dblw, dblw, !dblw };
    bool hdblset[5] = { dblh, dblh, dblh, !dblh, !dblh };
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
        VisRenderStatusLine();
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
                  VidmemPutEditorChar(MakeMenuColor(c), p);
        }   }   }}


        unsigned x=cancelx, xw=8, y=cancely;
        if(sel_y >= 0)
            { x  = options[sel_y].cx[sel_x];
              y  = options[sel_y].cy[sel_x];
              xw = options[sel_y].wid[sel_x];
            }
        VisPutCursorAt(x-1,y);
        if(FatMode) xw *= 2;
        {unsigned short* p = GetVidMem(x,y);
        for(unsigned m=0; m<xw; ++m)
            VidmemPutEditorChar(InvertColor(VidmemReadEditorChar(p)), p);}
    rewait:
        WaitInput(false);
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
    if(VidCellHeight != 8) DCPUpalette = false;
    return 1;
}

static int VerifyUnsavedExit(const char* action)
{
    if(!UnsavedChanges) return 1;
    VisSoftCursor(-1);
    int s = sprintf(StatusLine, "FILE IS UNSAVED. PROCEED WITH %s? Y/N  ", action);
    VisPutCursorAt(s-1, VidH-1);
    VisRenderTitleAndStatus();
    int decision = 0;
    for(;;)
    {
        WaitInput(true);
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
static int PromptText(const char* message, const char* deftext, char** result)
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
        VisRenderTitleAndStatus();
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
static void InvokeSave(int ask_name)
{
    if(ask_name || !CurrentFileName)
    {
        char* name = nullptr;
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
            char c = ExtractCharCode(EditLines[a][x]);
            if(c == '\n') fputc('\r', fp);
            fputc(c, fp);
        }
    }
    unsigned long size = ftell(fp);
    fclose(fp);
    sprintf(StatusLine, "Saved %lu bytes to %s", size, CurrentFileName);
    VisRenderTitleAndStatus();
    UnsavedChanges = false;
}
static inline void InvokeLoad()
{
    char* name = nullptr;
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
    UndoAppendOk=false;
}
static inline void LineAskGo() // Go to line
{
    unsigned DimY = VidH-1;
    char* line = nullptr;
    char Buf[64] = "";
    //sprintf(Buf, "%u", Cur.y + 1);
    int decision = PromptText("Goto line:", Buf, &line); // Warning: No range checking!
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
    VisRenderTitleAndStatus();
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

static inline void k_home(void)
{
    unsigned indent = 0;
    while(indent < EditLines[Cur.y].size()
       && ExtractCharCode(EditLines[Cur.y][indent]) == ' ') ++indent;
    // indent = number of spaces in the beginning of the line
    Cur.x = (Cur.x == indent ? 0 : indent);
}
static void k_end(void)
{
    Cur.x = EditLines[Cur.y].size();
    if(Cur.x > 0 && ExtractCharCode(EditLines[Cur.y].back()) == '\n')
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
static inline void k_ctrlleft(void)
{
    // Algorithm adapted from Joe 3.7
    #define at_line_end() (Cur.y >= EditLines.size() || Cur.x >= EditLines[Cur.y].size())
    #define at_begin() (Cur.x==0 && Cur.y==0)
    #define cur_ch     ExtractCharCode(EditLines[Cur.y][Cur.x])

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
static inline void k_ctrlright(void)
{
    // Algorithm adapted from Joe 3.7
    #define at_line_end() (Cur.y >= EditLines.size() || Cur.x >= EditLines[Cur.y].size())
    #define at_end()      (Cur.y >= EditLines.size() || ((Cur.y+1) == EditLines.size() && Cur.x == EditLines[Cur.y].size()))
    #define cur_ch        ExtractCharCode(EditLines[Cur.y][Cur.x])
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
    // Read the rest of mode settings
    // This should really be in vga.cc, but these assign
    // global variables that are _not_ indicative of current mode,
    // but dictate the next mode that will be set.
    // (Although at this point in file they _are_ indicative of current mode.)
    outportb(0x3C4, 1); use9bit = !(inportb(0x3C5) & 1);
    outportb(0x3C4, 1); dblw    = (inportb(0x3C5) >> 3) & 1;
    outportb(0x3D4, 9); dblh    = inportb(0x3D5) >> 7;
#endif

    unsigned long StatusLineProtection = MarioTimer + 200u;

    for(;;)
    {
        // Render / idle until a key is pressed
        WaitInput();

        // Identify the key that was pressed
        unsigned c = getch();

        // Read also the keyboard shift-key state, because this
        // is not necessarily indicated in the getch() result
        // in case of some ctrl-keycombinations
#ifdef __BORLANDC__
        int shift = 3 & (*(char*)MK_FP(0x40,0x17));
#elif defined(__DJGPP__)
        int shift = 3 & _farpeekb(_dos_ds, 0x417);
#else
        int shift = 0;
#endif

        // If there is a status line visible, and its expiration timer
        // has run out, delete the status line.
        if(StatusLine[0] && StatusLineProtection < MarioTimer)
        {
            StatusLine[0] = '\0';
            VisRender();
        }

        bool wasbegin = Cur.x==BlockBegin.x && Cur.y==BlockBegin.y;
        bool wasend   = Cur.x==BlockEnd.x   && Cur.y==BlockEnd.y;
        unsigned WasX = Cur.x, WasY = Cur.y;
        bool dragalong=false;

        unsigned /*DimX = VidW,*/ DimY = VidH-1;
        bool WasAppend = false;
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
                if(ENABLE_DRAG && shift) dragalong = true;
                break;
            }
            case CTRL('U'): // ctrl-U
            {
            pgup:;
                unsigned offset = Cur.y - Win.y;
                if(Cur.y > DimY) Cur.y -= DimY; else Cur.y = 0;
                Win.y = (Cur.y > offset) ? Cur.y-offset : 0;
                if(ENABLE_DRAG && shift) dragalong = true;
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
                        EditorCharVecType block, empty;
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
                        EditorCharVecType block;
                        GetBlock(block);
                        unsigned x = Cur.x, y = Cur.y;
                        PerformEdit(Cur.x,Cur.y, InsertMode?0u:block.size(), block);
                        BlockBegin.x = x; BlockBegin.y = y;
                        BlockEnd.x = Cur.x; BlockEnd.y = Cur.y;
                        break;
                    }
                    case 'y': case 'Y': case CTRL('Y'): // delete block
                    {
                        EditorCharVecType block, empty;
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
                        unsigned charcode = ExtractCharCode(EditLines[Cur.y][Cur.x]);
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
                        PerformEdit(InsertMode?0u:((c&0xFF)?1:2), 1u,c&0xFF,  (c&0xFF)?0u:1u, c>>8);
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
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    case 'P': // down
                        if(Cur.y+1 < EditLines.size()) ++Cur.y;
                        if(Cur.y >= Win.y+DimY) Win.y = Cur.y - DimY+1;
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    case 0x47: // home
                    {
                    home:
                        k_home();
                        Win.x = 0;
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    }
                    case 0x4F: // end
                    end:
                        k_end();
                        Win.x = 0;
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    case 'K': // left
                    {
                    lt_key:
                        k_left();
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    }
                    case 'M': // right
                    {
                    rt_key:
                        k_right();
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    }
                    case 0x73: // ctrl-left (go left on word boundary)
                    {
                        k_ctrlleft();
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    }
                    case 0x74: // ctrl-right (go right on word boundary)
                    {
                        k_ctrlright();
                        if(ENABLE_DRAG && shift) dragalong = true;
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
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    case 0x76: // ctrl-pgdn = goto end of file
                    ctrlpgdn:
                        Cur.y = EditLines.size()-1;
                        Win.y = 0;
                        if(Cur.y >= Win.y+DimY) Win.y = Cur.y - DimY+1;
                        goto end;
                    case 0x77: // ctrl-home = goto beginning of window (vertically)
                        Cur.y = Win.y;
                        if(ENABLE_DRAG && shift) dragalong = true;
                        break;
                    case 0x75: // ctrl-end = goto end of window (vertically)
                        Cur.y = Win.y + VidH-1;
                        if(ENABLE_DRAG && shift) dragalong = true;
                        // Hide the status line
                        StatusLine[0] = '\0';
                        break;
                    case 0x53: // delete
                    delkey:
                    {
                        unsigned eol_x = EditLines[Cur.y].size();
                        if(eol_x > 0 && ExtractCharCode(EditLines[Cur.y].back()) == '\n') --eol_x;
                        if(Cur.x > eol_x) { Cur.x = eol_x; break; } // just do end-key
                        PerformEdit(1u, 0u, nullptr);
                        WasAppend = true;
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
                        if(dblw) { dblw=false; VidW*=2; }
                        else     { dblw=true;  VidW/=2; VidW&=~1; }
                        goto newmode; // F6
                    case 0x59: shiftF6:
                               dblw = !dblw; goto newmode; // shift-F6
                    case 0x41: if(shift) goto shiftF7;
                        if(dblh) { dblh=false; if(VidCellHeight==8 || VidCellHeight==16)
                                                   VidCellHeight *= 2;
                                               else
                                                   VidH*=2; }
                        else    { dblh=true; if(VidCellHeight==16 || (VidCellHeight==32 && !FatMode))
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
                                             1);
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
                                    VidW * (use9bit ? 9 : 8) * (1+int(dblw)) * (FatMode?2:1),
                                    VidH * VidCellHeight * (1+int(dblh)),
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
                        VisRenderTitleAndStatus();
                        StatusLineProtection = MarioTimer + 200u;
                        break;
                    case 0x43: // F9
                        C64palette = !C64palette;
                        DispUcase  = C64palette;
                        if(C64palette) use9bit = false;
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
                PerformEdit(EditLines[Cur.y].size(), 0u, nullptr);
                break;
            }
            case CTRL('H'): // backspace = left + delete
            {
                unsigned nspaces = 0;
                while(nspaces < EditLines[Cur.y].size()
                   && ExtractCharCode(EditLines[Cur.y][nspaces]) == ' ') ++nspaces;
                if(nspaces > 0 && Cur.x == nspaces)
                {
                    nspaces = 1 + (Cur.x-1) % TabSize;
                    Cur.x -= nspaces;
                    PerformEdit(nspaces, 0u, nullptr);
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
                    PerformEdit(1u, 0u, nullptr);
                }
                WasAppend = true;
                break;
            }
            case CTRL('D'):
                goto delkey;
            case CTRL('I'):
            {
                unsigned nspaces = TabSize - Cur.x % TabSize;
                PerformEdit(InsertMode?0u:nspaces, nspaces,' ');
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
                       && ExtractCharCode(EditLines[Cur.y][nspaces]) == ' ') ++nspaces;
                    if(Cur.x < nspaces) nspaces = Cur.x;
                }
                PerformEdit(InsertMode?0u:1u, 1,'\n', nspaces,' ');
                //Win.x = 0;
                WasAppend = true;
                break;
            }
            default:
            {
                PerformEdit(InsertMode?0u:1u, 1, c);
                WasAppend = true;
                break;
            }
        }
        UndoAppendOk = WasAppend;
        if(dragalong)
        {
            bool dirty=false;
            if(wasbegin || !wasend) { BlockBegin.x=Cur.x; BlockBegin.y=Cur.y; dirty=true; }
            if(!wasbegin)           { BlockEnd.x  =Cur.x; BlockEnd.y  =Cur.y; dirty=true; }
            if( !wasbegin && !wasend
            && BlockBegin.x==BlockEnd.x
            && BlockBegin.y==BlockEnd.y) { BlockEnd.x=WasX; BlockEnd.y=WasY; }
            if(BlockBegin.y > BlockEnd.y
            || (BlockBegin.y == BlockEnd.y && BlockBegin.x > BlockEnd.x))
               {{ unsigned tmp = BlockBegin.y; BlockBegin.y=BlockEnd.y; BlockEnd.y=tmp; }
                { unsigned tmp = BlockBegin.x; BlockBegin.x=BlockEnd.x; BlockEnd.x=tmp; }}
            if(dirty) VisRender();
        }
    }
exit:;
    Cur.x = 0; Cur.y = Win.y + VidH-2; InsertMode = true;
    if(FatMode || C64palette)
    {
        FatMode    = false;
        C64palette = false;
        VgaSetCustomMode(VidW,VidH, VidCellHeight, use9bit, dblw, dblh, 1);
    }
    VisSetCursor();
#if defined(__BORLANDC__) || defined(__DJGPP__)
    DeInstallMario();
#endif
    exit(0);
    return 0;
}
