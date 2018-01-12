#ifdef __BORLANDC__
extern "C" {
extern _fastcall double CPUinfo(void);
}
#elif defined(__DJGPP__)

static double CPUinfo(void)
{
    unsigned value = 1234;
    __asm__ volatile("mov %%tr2, %0" : "=a"(value));
    return value * 1e3;
}

#else

static double CPUinfo(void)
{
    return 0;
}
#endif
