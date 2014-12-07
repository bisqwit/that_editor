#include "printf.hh"
#include <sstream>
#include <iomanip>

static const char DigitBufUp[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static const char DigitBufLo[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

template<typename CT>
void PrintfFormatter::MakeFrom(const std::basic_string<CT>& format)
{
    for(std::size_t b = format.size(), a = 0; a < b; ++a)
    {
        CT c = format[a];
        if(c == '%')
        {
            std::size_t percent_begin = a;

            arg argument;

            if(a < format.size() && format[a] == '-') { argument.leftalign = true; ++a; }
            if(a < format.size() && format[a] == '+') { argument.sign      = true; ++a; }
            if(a < format.size() && format[a] == '0') { argument.zeropad   = true; ++a; }
            if(a < format.size() && format[a] == '*') { argument.param_minwidth = true; ++a; }
            else while(a < format.size() && (format[a] >= '0' && format[a] <= '9'))
                argument.min_width = argument.min_width*10 + (format[a++] - '0');

            if(a < format.size() && format[a] == '.')
            {
                argument.max_width = 0;
                if(a < format.size() && format[a] == '*')
                    { argument.param_maxwidth = true; ++a; }
                else while(a < format.size() && (format[a] >= '0' && format[a] <= '9'))
                    argument.max_width = argument.max_width*10 + (format[a++] - '0');
            }

       another_formatchar:
            if(a >= format.size()) goto invalid_format;
            switch(format[a++])
            {
                case 'z':
                case 'l': goto another_formatchar; // ignore 'l' or 'z'
                case 'S':
                case 's': argument.format = arg::as_string; break;
                case 'C':
                case 'c': argument.format = arg::as_char; break;
                case 'x': argument.base   = arg::hex;
                          argument.format = arg::as_int; break;
                case 'X': argument.base   = arg::hexup;
                          argument.format = arg::as_int; break;
                case 'o': argument.base   = arg::oct;
                          argument.format = arg::as_int; break;
                case 'b': argument.base   = arg::bin;
                          argument.format = arg::as_int; break;
                case 'i':
                case 'u':
                case 'd': argument.format = arg::as_int; break;
                case 'g':
                case 'e':
                case 'f': argument.format = arg::as_float; break;
                default:
            invalid_format:
                    a = percent_begin;
                    trail += '%';
                    continue;
            }

            argument.before.swap(trail);
            formats.push_back(argument);
        }
        else
            trail += c;
    }
}

// When no parameters are remaining
void PrintfFormatter::Execute(PrintfFormatter::State& state)
{
    std::size_t position = state.position, pos = position / 4,  subpos = position % 4;
    if(subpos) ++pos;

    while(pos < formats.size())
    {
        state.result.append(formats[pos].before);
        state.result.append( (std::size_t) formats[pos].min_width, L' ' );
    }
    state.result += trail;
}

namespace
{
    template<typename T, bool Signed = std::is_signed<T>::value>
    struct IsNegative
    {
        static bool Test(T value) { return value < 0; }
    };
    template<typename T>
    struct IsNegative<T, false>
    {
        static bool Test(T) { return false; }
    };

    template<typename T, bool IsIntegral = std::is_integral<T>::value,
                         bool IsFloat    = std::is_floating_point<T>::value>
    class Categorize { };//: public std::integral_constant<int,0> { };

    // 1 = integers
    template<typename T> class Categorize<T,true,false>: public std::integral_constant<int,1> { };

    // 2 = floats
    template<typename T> class Categorize<T,false,true>: public std::integral_constant<int,2> { };

    // 3 = character pointers
    template<> class Categorize<const char*,false,false>: public std::integral_constant<int,3> { };
    template<> class Categorize<const wchar_t*,false,false>: public std::integral_constant<int,3> { };
    template<> class Categorize<const char16_t*,false,false>: public std::integral_constant<int,3> { };
    template<> class Categorize<const char32_t*,false,false>: public std::integral_constant<int,3> { };
    template<> class Categorize<const signed char*,false,false>: public std::integral_constant<int,3> { };
    template<> class Categorize<const unsigned char*,false,false>: public std::integral_constant<int,3> { };

    // 4 = strings
    template<typename T> class Categorize<std::basic_string<T>,false,false>: public std::integral_constant<int,4> { };


    template<typename T, int category = Categorize<typename std::remove_cv<
                                                   typename std::remove_reference<T>::type>::type
                                                  >::value>
    struct PrintfFormatDo { };

    template<typename T>
    struct PrintfFormatDo<T, 1> // ints
    {
        static void Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part);
        static T IntValue(T part) { return part; }
    };
    template<typename T>
    struct PrintfFormatDo<T, 2> // floats
    {
        static void Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part);
        static long long IntValue(T part) { return part; }
    };
    template<typename T>
    struct PrintfFormatDo<T, 3> // char-pointers
    {
        typedef typename std::remove_cv<
                typename std::remove_reference<
                                        decltype(*(T()))
                                       >::type>::type ctype;

        static void Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part);
        static void DoString(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part, std::size_t length);
        static long long IntValue(T part);
    };
    template<typename T>
    struct PrintfFormatDo<T, 4> // strings
    {
        using ctype = typename T::value_type;

        static void Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, const T& part);
        static long long IntValue(const T& part);
    };

    template<typename T>
    void PrintfFormatDo<T,1>::Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part)
    {
        /// Integer version
        switch(arg.format)
        {
            case PrintfFormatter::argsmall::as_char:
            {
                // Interpret as character
                char32_t n = part;
                PrintfFormatDo<const char32_t*>::DoString(arg, result, &n, 1);
                break;
            }

            case PrintfFormatter::argsmall::as_string:
            case PrintfFormatter::argsmall::as_int:
            case PrintfFormatter::argsmall::as_float:
                std::string s;
                // Use this contrived expression rather than "part < 0"
                // to avoid a compiler warning about expression being always false
                // due to limited datatype (when instantiated for unsigned types)
                if(IsNegative<T>::Test(part))
                                  { s += '-'; part = -part; }
                else if(arg.sign)   s += '+';

                std::string digitbuf;

                const char* digits = (arg.base & 64) ? DigitBufUp : DigitBufLo;
                int base = arg.base & ~64;
                while(part != 0)
                {
                    digitbuf += digits[ part % base ];
                    part /= base;
                }

                // Append the digits in reverse order
                for(std::size_t a = digitbuf.size(); a--; )
                    s += digitbuf[a];
                if(digitbuf.empty())
                    s += '0';

                // Delegate the width-formatting
                arg.max_width = ~0u;
                PrintfFormatDo<const char*>::DoString(arg, result, s.data(), s.size());
                break;
        }
    }

    template<typename T>
    void PrintfFormatDo<T,2>::Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part)
    {
        // Float version
        switch(arg.format)
        {
            case PrintfFormatter::argsmall::as_char:
            {
                // Cast into integer, and interpret as character
                char32_t n = part;
                PrintfFormatDo<const char32_t*>::DoString(arg, result, &n, 1);
                break;
            }
            case PrintfFormatter::argsmall::as_int:
                // Cast into integer, and interpret
                PrintfFormatDo<long long>::Do(
                    arg, result,
                    (long long)(part) );
                break;

            case PrintfFormatter::argsmall::as_string:
            case PrintfFormatter::argsmall::as_float:
                // Print float
                // TODO: Handle different formatting styles (exponents, automatic precisions etc.)
                //       better than this.
                std::stringstream s;
                if(!(part < 0) && arg.sign) s << '+';
                s << std::setprecision( arg.max_width);
                s << part;
                arg.max_width = ~0u;
                std::string s2 = s.str();
                PrintfFormatDo<const char*>::DoString(arg, result, s2.data(), s2.size());
                break;
        }
    }

    template<typename T>
    void PrintfFormatDo<T,3>::Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, T part)
    {
        // Character pointer version
        std::size_t length = 0;
        for(const ctype* p = part; *p; ++p) ++length;

        switch(arg.format)
        {
            case PrintfFormatter::argsmall::as_char:
            case PrintfFormatter::argsmall::as_string:
                // Do with a common function for char* and basic_string
                DoString(arg, result, part, length);
                break;

            case PrintfFormatter::argsmall::as_int:
            case PrintfFormatter::argsmall::as_float:
                // Cast string into integer!
                // Delegate it...
                PrintfFormatDo<std::basic_string<ctype>>::Do(
                    arg, result,
                    std::basic_string<ctype>(part, length) );
                break;
        }
    }

    template<typename T>
    void PrintfFormatDo<T,3>::DoString(
        PrintfFormatter::argsmall& arg,
        std::basic_string<char32_t> & result,
        T part,
        std::size_t length)
    {
        // Character pointer version
        if(length > arg.max_width) length = arg.max_width;

        result.reserve( result.size() + (length < arg.min_width ? arg.min_width : length) );

        char32_t pad = arg.zeropad ? L'0' : L' ';

        if(length < arg.min_width && !arg.leftalign)
            result.append( std::size_t(arg.min_width-length), pad );

        for( std::size_t a=0; a<length; ++a)
            result.push_back( char32_t( part[a] ) );

        if(length < arg.min_width && arg.leftalign)
            result.append( std::size_t(arg.min_width-length), pad );
    }

    template<typename T>
    long long PrintfFormatDo<T,3>::IntValue(T part)
    {
        // Delegate it
        return PrintfFormatDo<std::basic_string<ctype>>::IntValue( std::basic_string<ctype>(part) );
    }

    template<typename T>
    void PrintfFormatDo<T,4>::Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t> & result, const T& part)
    {
        // String version
        switch(arg.format)
        {
            case PrintfFormatter::argsmall::as_char:
            case PrintfFormatter::argsmall::as_string:
                // Do with a common function for char* and basic_string
                PrintfFormatDo<const ctype*>::DoString(arg, result, part.data(), part.size() );
                break;

            case PrintfFormatter::argsmall::as_int:
                // Cast string into integer!
                PrintfFormatDo<long long>::Do(arg, result, IntValue(part));
                break;

            case PrintfFormatter::argsmall::as_float:;
                // Cast string into float!
                std::basic_stringstream<ctype> s;
                double d = 0.;
                s << part;
                s >> d;
                PrintfFormatDo<double>::Do(arg, result, d);
                break;
        }
    }

    template<typename T>
    long long PrintfFormatDo<T,4>::IntValue(const T& part)
    {
        // Delegate it
        std::basic_stringstream<ctype> s;
        long long l = 0;
        s << part;
        s >> l;
        return l;
    }
}

template<typename T>
void PrintfFormatter::ExecutePart(PrintfFormatter::State& state, T part)
{
    std::size_t position = state.position, pos = position / 4,  subpos = position % 4;
    if(pos >= formats.size()) return;

    using TT =
        typename std::remove_cv<
        typename std::remove_reference<T>::type>::type;

    if(subpos == 0)
    {
        state.result    += formats[pos].before;
        state.minwidth  = formats[pos].min_width;
        state.maxwidth  = formats[pos].max_width;
        state.leftalign = formats[pos].leftalign;
    }

    if(subpos == 0)
    {
        if(formats[pos].param_minwidth)
        {
            // This param should be an integer.
            auto p = PrintfFormatDo<TT>::IntValue(part);
            // Use this contrived expression rather than "p < 0"
            // to avoid a compiler warning about expression being always false
            // due to limited datatype (when instantiated for unsigned types)
            if(IsNegative<decltype(p)>::Test(p))
            {
                state.leftalign = true;
                state.minwidth  = -p;
            }
            else
                state.minwidth = p;

            state.position = pos*4 + 1;
            return;
        }
        goto pos1;
    }
    if(subpos == 1) pos1:
    {
        if(formats[pos].param_maxwidth)
        {
            // This param should be an integer.
            state.maxwidth = PrintfFormatDo<TT>::IntValue(part);
            state.position = pos*4 + 2;
            return;
        }
        //goto pos2;
    }
    //if(subpos == 2) pos2:
    //{
    argsmall a { state.minwidth,
                 state.maxwidth,
                 state.leftalign,
                 formats[pos].sign,
                 formats[pos].zeropad,
                 formats[pos].base,
                 formats[pos].format };
    PrintfFormatDo<TT>::Do(a, state.result, part);

    state.position = (pos+1)*4;
    //}
}

template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char16_t);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char32_t);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, short);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, int);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned char);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned short);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned int);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned long long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, bool);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, float);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, double);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, const char*);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, const std::string&);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, const std::basic_string<char32_t>&);

template void PrintfFormatter::MakeFrom(const std::basic_string<char>&);
template void PrintfFormatter::MakeFrom(const std::basic_string<char32_t>&);
