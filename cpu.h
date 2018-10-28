#ifdef __BORLANDC__
extern "C" {
extern _fastcall double CPUinfo(void);
}
#elif defined(__DJGPP__)

static double CPUinfo(void)
{
    /* HUGE WARNING: THIS *REQUIRES* A PATCHED DOSBOX,
     * UNPATCHED DOSBOXES WILL TRIGGER AN EXCEPTION HERE */
    unsigned value = 1234;
    __asm__ volatile("mov %%tr2, %0" : "=a"(value));
    return value * 1e3;
}

#else

static double CPUinfo(void)
{
    char Buf[1024];
    FILE* fp = fopen("/proc/cpuinfo", "rt");
    if(!fp) return 0;
    unsigned tot = 0;
    double avg_mhz=0;
    while(fgets(Buf, sizeof(Buf), fp))
        if(strncmp(Buf, "cpu MHz", 7) == 0)
        {
            const char* p = Buf;
            while(*p && *p != ':') ++p;
            while(*p && *p == ':') ++p;
            avg_mhz += strtod(p, nullptr) * 1e6;
            ++tot;
        }
    fclose(fp);
    return tot ? avg_mhz / tot : 0;
}
#endif
