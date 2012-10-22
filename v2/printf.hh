#ifndef bqtEprintfHH
#define bqtEprintfHH

#include <string>
#include <vector>
#include <type_traits>

class PrintfFormatter
{
public: // ...blah (must be public because of PrintfFormatDo in printf.cc)
    struct argsmall
    {
        unsigned min_width  = 0,     max_width      = ~0u;
        bool leftalign      = false, sign           = false,  zeropad = false;
        enum basetype   : char { decimal=10, hex=16,  hexup=16+64,  oct=8, bin=2 } base   = decimal;
        enum formattype : char { as_char, as_int, as_float, as_string } format = as_string;

        argsmall() { }
        argsmall(unsigned mi,unsigned ma,bool la, bool si, bool zp, basetype b, formattype f)
            : min_width(mi), max_width(ma), leftalign(la), sign(si), zeropad(zp), base(b), format(f) { }
    };
    struct arg: public argsmall
    {
        std::basic_string<char32_t> before;
        bool param_minwidth = false, param_maxwidth = false;
    };
    struct State
    {
        std::size_t position = 0;
        unsigned    minwidth = 0;
        unsigned    maxwidth = 0;
        bool        leftalign = false;
        std::basic_string<char32_t> result;
    };

    std::vector<arg>            formats;
    std::basic_string<char32_t> trail;

public:
    template<typename C>
    void MakeFrom(const std::basic_string<C>& format);

    template<typename CT>
    void MakeFrom(const CT* format)
    {
        MakeFrom( std::basic_string<CT> (format) );
    }

    template<typename T, typename... T2>
    void Execute(State& state, const T& a, T2... rest)
    {
        // TODO: Use is_trivially_copyable rather than is_pod,
        //       once GCC supports it
        typedef typename std::conditional<std::is_pod<T>::value, T, const T&>::type TT;
        ExecutePart<TT>(state, a);
        Execute(state, rest...);
    }

    // When no parameters are remaining
    void Execute(State& state);

private:
    template<typename T>
    void ExecutePart(State& state, T part);
};

template<typename... T>
std::basic_string<char32_t> Printf(PrintfFormatter& fmt, T... args)
{
    PrintfFormatter::State state;
    fmt.Execute(state, args...);
    return state.result;
}


template<typename CT, typename... T>
std::basic_string<char32_t>
    Printf(const std::basic_string<CT>& format, T... args)
{
    PrintfFormatter Formatter;
    Formatter.MakeFrom(format);
    return Printf(Formatter, args...);
}

template<typename CT, typename... T>
std::basic_string<char32_t>
    Printf(const CT* format, T... args)
{
    PrintfFormatter Formatter;
    Formatter.MakeFrom(format);
    return Printf(Formatter, args...);
}

#endif
