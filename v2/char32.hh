#ifndef bqtEChar32HH
#define bqtEChar32HH

extern char UnicodeToASCIIapproximation(char32_t ch);
extern bool isalnum(char32_t ch);

/* Simple char32 - char8 converters.
 * This works nicely for ISO-8859-1 but not for anything else.
 * A future version might have here a call to iconv() or something like that.
 */
class Char32Reader
{
    char32_t buffer;
public:
    void Put(unsigned char c) { buffer=c; }
    void EndPut()             { }
    bool Avail() const        { return buffer != ~char32_t(0); }
    char32_t Get()            { auto r = buffer; buffer = ~char32_t(0); return r; }
};

class Char32Writer
{
    char32_t buffer;
public:
    void Put(char32_t c) { buffer = c; }
    void EndPut()        { buffer = ~char32_t(0); }
    bool Avail() const   { return buffer != ~char32_t(0); }
    unsigned char Get()  { auto r = buffer; buffer = ~char32_t(0); return r; }
};

#endif
