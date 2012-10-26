#include <cctype>
#include "char32.hh"

char UnicodeToASCIIapproximation(char32_t ch)
{
    // Convert an unicode character into an ASCII character representing
    // the approximate shape and function of the character.
    // The outcome should be a value in range 0-255, useful for a lookup
    // table in the syntax highlighter.

    // TODO: Extend
    if(ch < 256)   return ch;
    if(ch < 0x2B0) return 'a';
    if(ch < 0x370) return ' ';
    if(ch < 0x2B0) return 'a';

    return '?';
}

bool isalnum(char32_t ch)
{
    return std::isalnum(UnicodeToASCIIapproximation(ch));
}

