#include <stdio.h>
#include <dos.h> // for MK_FP

#include "vectype.hh"
#include "jsf.hh"

unsigned short _far* VidMem = (unsigned short _far*) MK_FP(0xB800, 0x0000);
unsigned char VidW, VidH;

const unsigned MaxEditBufferSize = 64000;
unsigned char _far EditBuffer[MaxEditBufferSize];
unsigned char _far ColorBuffer[MaxEditBufferSize];

JSF     Syntax;

size_t  WindowPos; // Points to beginning of line
size_t  EditPos;
size_t  GapBegin;
size_t  GapEnd;
size_t  WinX, WinY;
size_t  CurX, CurY;

void FileLoad(char* fn)
{
    fprintf(stderr, "Loading '%s'...\n", fn);
    FILE* fp = fopen(fn, "rb");
    if(!fp) { perror(fn); return; }
    fseek(fp, 0, SEEK_END);
    rewind(fp);
    size_t target = 0;
    for(;;)
    {
        size_t limit = MaxEditBufferSize - target;
        size_t r = fread(&EditBuffer[target], 1, limit, fp);
        if(r == 0) break;
        target += r;
    }
    fclose(fp);
    GapBegin  = target;
    GapEnd    = MaxEditBufferSize;
    EditPos   = 0;
    WinX = WinY = 0;
    CurX = CurY = 0;
}
void FileNew()
{
    GapBegin = 0;
    GapEnd   = MaxEditBufferSize;
    EditPos  = 0;
    WinX = WinY = 0;
    CurX = CurY = 0;
}
struct ApplyEngine: public JSF::Applier
{
    size_t pos;
    ApplyEngine() : pos(0) { }
    virtual int Get(void)
    {
        if(pos >= MaxEditBufferSize) return -1;
        if(pos >= GapBegin && pos < GapEnd) pos = GapEnd;
        return EditBuffer[pos++];
    }
    virtual void Recolor(unsigned n, unsigned attr)
    {
        size_t p = pos;
        for(; n > 0; --n)
        {
            if(p == GapEnd) p = GapBegin;
            ColorBuffer[--p] = attr;
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
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl }
    if(VidH == 0) VidH = 25; else VidH += 1;
}
void VisRender()
{
    for(unsigned y=0; y<VidH; ++y)
    {
        
    }
}

int main()
{
    Syntax.Parse("c.jsf");
    FileLoad("../raytrace/trace.cpp");
    FileColorize();
    VisGetGeometry();
    VisRender();
}
