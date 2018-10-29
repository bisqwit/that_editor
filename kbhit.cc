#include "kbhit.hh"
#include "langdefs.hh"

#undef kbhit
#undef getch

#ifdef __BORLANDC__
# include <dos.h> // for MK_FP
# include <conio.h>

static unsigned char KbStatus = 0;
int MyKbhit()
{
    if(!KbStatus)
    {
        _asm { mov ah, 0x01; int 0x16; jnz keyfound }
        return 0;
    }
keyfound:
    return 1;
}
int MyGetch()
{
    register unsigned char r = KbStatus;
    if(r) { KbStatus=0; return r; }
    _asm { xor ax,ax; int 0x16 }
    if(_AL) return _AL;
    KbStatus    = _AH;
    return 0;
}
void KbIdle()
{
    _asm { hlt }
}

#elif defined(__DJGPP__)

# include <dos.h>

static unsigned char KbStatus = 0;
int MyKbhit()
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
int MyGetch()
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
void KbIdle()
{
    //__dpmi_yield();
    __asm__ volatile("hlt");
    // dpmi_yield is not used, because it's not implemented in dosbox
    // Instead, we issue "hlt" and patch DOSBox to not produce an exception
    /* HUGE WARNING: THIS *REQUIRES* A PATCHED DOSBOX,
     * UNPATCHED DOSBOXES WILL TRIGGER AN EXCEPTION HERE */
}

#else // not borlandc, djgpp

#if 0
static unsigned      kbhitptr   = 0;
static unsigned char kbhitbuf[] =
{
    // simulated input for testing with valgrind
    '','L', '1','0','0','0','\n',
    '','k',
    '','u',
    '','b',
    '','y'
};
int MyKbhit() { return (kbhitptr < sizeof(kbhitbuf) && std::rand()%100 == 0); }
int MyGetch() { return kbhitbuf[kbhitptr++]; }

#else
#include <SDL.h>
#include <cstdlib>
#include <string>

#define CTRL(c) ((c) & 0x1F)

static std::string pending_input;
bool kbshift=false;
static Uint32 last_ev_time=0, rewind_ptr=0;

extern void VgaRedraw(int);

static void ProcessEvent(SDL_Event& ev)
{
    //fprintf(stderr, "ev type %u\n", ev.type);
    switch(ev.type)
    {
        case SDL_WINDOWEVENT:
            //fprintf(stderr, "window event %d\n", ev.window.event);
            switch(ev.window.event)
            {
                case SDL_WINDOWEVENT_EXPOSED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    VgaRedraw(1);
                default:
                    break;
            }
            break;
        case SDL_QUIT:
            pending_input += CTRL('c');
            break;
        case SDL_KEYDOWN:
        {
            /*fprintf(stderr, "Dn: state=%u repeat=%u time=%u sym=%u scancode=%u mod=0x%X\n",
                ev.key.state, ev.key.repeat, ev.key.timestamp,
                ev.key.keysym.sym, ev.key.keysym.scancode, ev.key.keysym.mod);*/
            if(ev.key.repeat && ev.key.timestamp == last_ev_time)
            {
                pending_input.erase(rewind_ptr);
            }
            last_ev_time = ev.key.timestamp;
            rewind_ptr   = pending_input.size();
            bool alt   = ev.key.keysym.mod & KMOD_LALT;
            bool altgr = ev.key.keysym.mod & KMOD_RALT;
            bool ctrl  = ev.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL);
            bool shift = ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
            bool caps  = ev.key.keysym.mod & KMOD_CAPS;
            switch(ev.key.keysym.scancode)
            {
                case SDL_SCANCODE_LSHIFT: kbshift=true; break;
                case SDL_SCANCODE_RSHIFT: kbshift=true; break;
                #define ALPHA(c, rsym,shiftsym, ctrlsym, grsym, altsym) \
                    case c: \
                        if(altgr || (alt&&ctrl)) pending_input += grsym;    \
                        else if(shift==!caps)    pending_input += shiftsym; \
                        else if(alt)             pending_input += altsym;   \
                        else if(ctrl)            pending_input += ctrlsym;  \
                        else                     pending_input += rsym; \
                        break;
                ALPHA(SDL_SCANCODE_A,'a','A',    CTRL('A'), std::string_view("\0\36", 2), std::string_view("\0\36", 2))
                ALPHA(SDL_SCANCODE_B,'b','B',    CTRL('B'), std::string_view("\0\60", 2), std::string_view("\0\60", 2))
                ALPHA(SDL_SCANCODE_C,'c','C',    CTRL('C'), std::string_view("\0\56", 2), std::string_view("\0\56", 2))
                ALPHA(SDL_SCANCODE_D,'d','D',    CTRL('D'), std::string_view("\0\40", 2), std::string_view("\0\40", 2))
                ALPHA(SDL_SCANCODE_E,'e','E',    CTRL('E'), std::string_view("\325",  1), std::string_view("\0\22", 2))
                ALPHA(SDL_SCANCODE_F,'f','F',    CTRL('F'), std::string_view("\0\41", 2), std::string_view("\0\41", 2))
                ALPHA(SDL_SCANCODE_G,'g','G',    CTRL('G'), std::string_view("\0\42", 2), std::string_view("\0\42", 2))
                ALPHA(SDL_SCANCODE_H,'h','H',    CTRL('H'), std::string_view("\0\43", 2), std::string_view("\0\43", 2))
                ALPHA(SDL_SCANCODE_I,'i','I',    CTRL('I'), std::string_view("\0\27", 2), std::string_view("\0\27", 2))
                ALPHA(SDL_SCANCODE_J,'j','J',    CTRL('J'), std::string_view("\0\44", 2), std::string_view("\0\44", 2))
                ALPHA(SDL_SCANCODE_K,'k','K',    CTRL('K'), std::string_view("\0\45", 2), std::string_view("\0\45", 2))
                ALPHA(SDL_SCANCODE_L,'l','L',    CTRL('L'), std::string_view("\0\46", 2), std::string_view("\0\46", 2))
                ALPHA(SDL_SCANCODE_M,'m','M',    CTRL('M'), std::string_view("\346",  1), std::string_view("\0\62", 2))
                ALPHA(SDL_SCANCODE_N,'n','N',    CTRL('N'), std::string_view("\0\61", 2), std::string_view("\0\61", 2))
                ALPHA(SDL_SCANCODE_O,'o','O',    CTRL('O'), std::string_view("\0\30", 2), std::string_view("\0\30", 2))
                ALPHA(SDL_SCANCODE_P,'p','P',    CTRL('P'), std::string_view("\0\31", 2), std::string_view("\0\31", 2))
                ALPHA(SDL_SCANCODE_Q,'q','Q',    CTRL('Q'), std::string_view("\100",  1), std::string_view("\0\20", 2))
                ALPHA(SDL_SCANCODE_R,'r','R',    CTRL('R'), std::string_view("\0\23", 2), std::string_view("\0\23", 2))
                ALPHA(SDL_SCANCODE_S,'s','S',    CTRL('S'), std::string_view("\0\37", 2), std::string_view("\0\37", 2))
                ALPHA(SDL_SCANCODE_T,'t','T',    CTRL('T'), std::string_view("\0\24", 2), std::string_view("\0\24", 2))
                ALPHA(SDL_SCANCODE_U,'u','U',    CTRL('U'), std::string_view("\0\26", 2), std::string_view("\0\26", 2))
                ALPHA(SDL_SCANCODE_V,'v','V',    CTRL('V'), std::string_view("\0\57", 2), std::string_view("\0\57", 2))
                ALPHA(SDL_SCANCODE_W,'w','W',    CTRL('W'), std::string_view("\0\21", 2), std::string_view("\0\21", 2))
                ALPHA(SDL_SCANCODE_X,'x','X',    CTRL('X'), std::string_view("\0\55", 2), std::string_view("\0\55", 2))
                ALPHA(SDL_SCANCODE_Y,'y','Y',    CTRL('Y'), std::string_view("\0\25", 2), std::string_view("\0\25", 2))
                ALPHA(SDL_SCANCODE_Z,'z','Z',    CTRL('Z'), std::string_view("\0\54", 2), std::string_view("\0\54", 2))
                ALPHA(SDL_SCANCODE_0,'0','=',    CTRL('0'), std::string_view("\175",  1), std::string_view("\0\201",2))
                ALPHA(SDL_SCANCODE_1,'1','!',    CTRL('1'), std::string_view("\0\170",2), std::string_view("\0\170",2))
                ALPHA(SDL_SCANCODE_2,'2','"',    CTRL('2'), std::string_view("\100",  1), std::string_view("\0\171",2))
                ALPHA(SDL_SCANCODE_3,'3','#',    CTRL('3'), std::string_view("\234",  1), std::string_view("\0\172",2))
                ALPHA(SDL_SCANCODE_4,'4','\317', CTRL('4'), std::string_view("\44",   1), std::string_view("\0\173",2))
                ALPHA(SDL_SCANCODE_5,'5','%',    CTRL('5'), std::string_view("\325",  1), std::string_view("\0\174",2))
                ALPHA(SDL_SCANCODE_6,'6','&',    CTRL('6'), std::string_view("\0\175",2), std::string_view("\0\175",2))
                ALPHA(SDL_SCANCODE_7,'7','/',    CTRL('7'), std::string_view("\173",  1), std::string_view("\0\176",2))
                ALPHA(SDL_SCANCODE_8,'8','(',    CTRL('8'), std::string_view("\133",  1), std::string_view("\0\177",2))
                ALPHA(SDL_SCANCODE_9,'9',')',    CTRL('9'), std::string_view("\135",  1), std::string_view("\0\200",2))
                //
                ALPHA(SDL_SCANCODE_COMMA, ',',';',    CTRL(','), std::string_view("\0\74", 2), std::string_view("\0\74",2))
                ALPHA(SDL_SCANCODE_PERIOD,'.',':',    CTRL(','), std::string_view("\0\76", 2), std::string_view("\0\76",2))
                ALPHA(SDL_SCANCODE_SLASH, '-','_',    CTRL(','), std::string_view("\0\75", 1), std::string_view("\0\75",2))
                //
                ALPHA(SDL_SCANCODE_SEMICOLON, '\224', '\231', CTRL(','), std::string_view("\0\47", 2), std::string_view("\0\47",2))
                ALPHA(SDL_SCANCODE_APOSTROPHE,'\204', '\216', CTRL(','), std::string_view("\0\48", 2), std::string_view("\0\48",2))
                ALPHA(SDL_SCANCODE_BACKSLASH, '\'',   '*',    CTRL(','), std::string_view("\0\49", 2), std::string_view("\0\49",2))
                //
                ALPHA(SDL_SCANCODE_LEFTBRACKET, '\206', '\217', CTRL(','), std::string_view("\0\32", 2), std::string_view("\0\32",2))
                ALPHA(SDL_SCANCODE_RIGHTBRACKET,'"',    '^',    CTRL(','), std::string_view("\0\33", 2), std::string_view("\0\33",2))
                //
                ALPHA(SDL_SCANCODE_MINUS,  '+', '?',   CTRL(','), std::string_view("\134",  1), std::string_view("\0\202",2))
                ALPHA(SDL_SCANCODE_EQUALS, '\'','`',   CTRL(','), std::string_view("\174",  1), std::string_view("\0\203",2))
                ALPHA(SDL_SCANCODE_SPACE,  ' ', ' ',   ' ', " ", " ")
                #undef ALPHA
                //
                #define FUNC(n, plainsym, shiftsym, altsym, ctrlsym) \
                    case SDL_SCANCODE_##n: \
                        if(alt||altgr)   pending_input += std::string_view("\0" altsym,2); \
                        else if(ctrl)    pending_input += std::string_view("\0" ctrlsym,2); \
                        else if(shift)   pending_input += std::string_view("\0" shiftsym,2); \
                        else             pending_input += std::string_view("\0" plainsym,2); \
                        break;
                FUNC(F1, "\73", "\124", "\150", "\136")
                FUNC(F2, "\74", "\125", "\151", "\137")
                FUNC(F3, "\75", "\126", "\152", "\140")
                FUNC(F4, "\76", "\127", "\153", "\141")
                FUNC(F5, "\77", "\130", "\154", "\142")
                FUNC(F6, "\100","\131", "\155", "\143")
                FUNC(F7, "\101","\132", "\156", "\144")
                FUNC(F8, "\102","\133", "\157", "\145")
                FUNC(F9, "\103","\134", "\160", "\146")
                FUNC(F10,"\104","\135", "\161", "\147")
                FUNC(F11,"\205","\207", "\213", "\211")
                FUNC(F12,"\206","\210", "\214", "\212")
                case SDL_SCANCODE_KP_7: FUNC(HOME,     "\107", "\107", "\227", "\167")
                case SDL_SCANCODE_KP_8: FUNC(UP,       "\110", "\110", "\230", "\215")
                case SDL_SCANCODE_KP_9: FUNC(PAGEUP,   "\111", "\111", "\231", "\204")
                case SDL_SCANCODE_KP_4: FUNC(LEFT,     "\113", "\113", "\233", "\163")
                FUNC(KP_5,                     "\114", "\114", "\234", "\217")
                case SDL_SCANCODE_KP_6: FUNC(RIGHT,    "\115", "\115", "\235", "\164")
                case SDL_SCANCODE_KP_1: FUNC(END,      "\117", "\117", "\237", "\165")
                case SDL_SCANCODE_KP_2: FUNC(DOWN,     "\120", "\120", "\240", "\221")
                case SDL_SCANCODE_KP_3: FUNC(PAGEDOWN, "\121", "\121", "\241", "\166")
                case SDL_SCANCODE_KP_0: FUNC(INSERT,   "\122", "\5",   "\242", "\4")
                case SDL_SCANCODE_KP_PERIOD: FUNC(DELETE,   "\123", "\7",   "\243", "\6")
                #undef FUNC
                case SDL_SCANCODE_BACKSPACE:
                    if(altgr||(alt&&ctrl)) pending_input += std::string_view("\0\11",2);
                    else if(alt)           pending_input += std::string_view("\0\10",2);
                    else pending_input += '\10';
                    break;
                case SDL_SCANCODE_TAB:
                    if(altgr||alt)      pending_input += std::string_view("\0\245",2);
                    else if(ctrl)       pending_input += std::string_view("\0\224",2);
                    else if(shift)      pending_input += std::string_view("\0\17",2);
                    else pending_input += '\11';
                    break;
                case SDL_SCANCODE_ESCAPE:
                    pending_input += '\33';
                    break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER:
                    if(ctrl) pending_input += '\n';
                    else     pending_input += '\r';
                    break;
                default: break;
            }
            break;
        }
        case SDL_KEYUP:
            switch(ev.key.keysym.scancode)
            {
                case SDL_SCANCODE_LSHIFT: kbshift=false; break;
                case SDL_SCANCODE_RSHIFT: kbshift=false; break;
                default: break;
            }
            break;
    }
}

int MyKbhit()
{
    if(!pending_input.empty())
        return true;

    SDL_Event ev;
    while(SDL_PollEvent(&ev))
        ProcessEvent(ev);

    return !pending_input.empty();
}
int MyGetch()
{
    while(pending_input.empty())
    {
        KbIdle();
    }
    unsigned char result = pending_input[0];
    pending_input.erase(0, 1);
    return result;
}
void KbIdle()
{
    if(!pending_input.empty()) return;

    VgaRedraw(0);

    // Wait for an event, update cursor and stuff though
    SDL_Event ev;
    if(SDL_WaitEventTimeout(&ev, 1000/60))
        do {
            ProcessEvent(ev);
        } while(SDL_PollEvent(&ev));
}

#endif

#endif
