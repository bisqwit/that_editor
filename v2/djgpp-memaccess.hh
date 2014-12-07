// DJGPP-specific include files, for accessing the screen & keyboard etc.:
#include <conio.h>        // For kbhit, getch, textmode (console access)
#include <dpmi.h>         // For __dpmi_int (mouse access)
#include <go32.h>         // For _dos_ds (VRAM access)
#include <sys/movedata.h> // For movedata (VRAM access)
#include <sys/farptr.h>   // for _farpeekl, _farpokew
#include <dos.h>          // For outportb

// In order to access BIOS timer and VGA memory:
#define Timer _farpeekl(_dos_ds, 0x46C)

template<typename BaseT, typename ElemT>
struct MemProxy: public BaseT
{
    MemProxy(unsigned o): BaseT{o} {}
    inline void operator|= (ElemT b) { *this = *this | b; }
    inline void operator&= (ElemT b) { *this = *this & b; }
};

struct MemByteProxyBase
{
    unsigned offset;
    void operator=(unsigned char value) const { _farpokeb(_dos_ds, offset, value); }
    operator unsigned char() const            { return _farpeekb(_dos_ds, offset); }
};
typedef MemProxy<MemByteProxyBase, unsigned char> MemByteProxy;

struct MemWordProxyBase
{
    unsigned offset;
    void operator=(unsigned short value) const { _farpokew(_dos_ds, offset, value); }
    operator unsigned short() const            { return _farpeekw(_dos_ds, offset); }
};
typedef MemProxy<MemWordProxyBase, unsigned char> MemWordProxy;

static struct
{
    MemWordProxy operator[] (unsigned offset) const
    {
        return MemWordProxy{0xB8000+2u*offset};
    }
} const VidMem = {}; // Simulate an array that is at B800:0000
