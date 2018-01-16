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

#define cdecl
#define register

#ifdef __DJGPP__

static unsigned char KbStatus = 0;
static int MyKbhit()
{
    if(!KbStatus)
    {
        REGS r{};
        r.h.ah = 1;
        int86(0x16, &r, &r);
        return (r.w.flags & 0x40) ? 0 : 1;
    }
    return 1;
}
static int MyGetch()
{
    unsigned char s = KbStatus;
    if(s) { KbStatus=0; return s; }
    REGS r{};
    r.w.ax = 0;
    int86(0x16, &r, &r);
    if(r.h.al) return r.h.al;
    KbStatus = r.h.ah;
    return 0;
}
#define kbhit MyKbhit
#define getch MyGetch

#else // not borlandc, djgpp

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

#endif
