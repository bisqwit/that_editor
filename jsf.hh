/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include "langdefs.hh"
#include <string.h>

#ifndef __BORLANDC__
# include <strings.h>
#else
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

#ifndef __BORLANDC__
# //define JSF_FORGO_SAVINGS
# include <string>
#endif

#include "vec_c.hh" // For the implementation of buffer
#include "vec_l.hh"

const char* sort_info;
char        sort_igncase;

#if defined(__cplusplus) && __cplusplus >= 199700L
template<class DerivedClass>
#endif
class JSF
#if defined(__cplusplus) && __cplusplus >= 199700L
          : public DerivedClass
#endif
{
public:
    JSF() : all_strings(),string_tables(),all_options(),all_states(),first_state(nullptr)
    {
        //fprintf(stdout, "JSF init... "); fflush(stdout);
        Clear();
        //fprintf(stdout, "done\n"); fflush(stdout);
    }
    ~JSF()
    {
        Clear();
    }
    void Parse(const char* fn)
    {
        FILE* fp = fopen(fn, "rb");
        if(!fp) { perror(fn); return; }
        Parse(fp);
        fclose(fp);
    }
private:
    // Removes comments and trailing space from the buffer
    static void cleanup(char* Buf)
    {
        char quote=0, *begin = Buf, *end = strchr(Buf, '\0');
        for(; *begin; ++begin)
        {
            if(*begin == '#' && !quote)
                { end=begin; *begin='\0'; break; }
            if(*begin == '"') quote=!quote;
            else if(*begin == '\\') ++begin;
        }
        while(end > Buf &&
            (end[-1] == '\r'
          || end[-1] == '\n'
          || end[-1] == ' '
          || end[-1] == '\t')) --end;
        *end = '\0';
    }
private:
    static int ItemCompare(const void* a, const void* b)
    {
        const unsigned long* ap = (const unsigned long*) a;
        const unsigned long* bp = (const unsigned long*) b;
        return sort_igncase ? strcasecmp(sort_info+ap[0], sort_info+bp[0])
                            : strcmp(sort_info+ap[0], sort_info+bp[0]);
    }
    static unsigned long AddString(CharVecType& vec, const char* namebegin, const char* nameend)
    {
      #if defined(__GNU_SOURCE)// && !defined(__DJGPP__)
        void* ptr = memmem(&vec[0], vec.size(), namebegin, nameend-namebegin+1);
        if(ptr) { return (const char*)ptr - (const char*)&vec[0]; }
      #endif
        // Adds a '\0' terminated string (*nameend must be '\0')
        unsigned long result = vec.size();
        vec.insert(vec.end(), (const unsigned char*)namebegin, (const unsigned char*)(nameend+1));
        return result;
    }
    static const unsigned long& BinarySearch(
        const LongVecType& data, const CharVecType& names,
        unsigned begin, unsigned end,
        const char* key, unsigned key_length,
        int (*comparator)(const char*,const char*, size_t))
    {
        static unsigned long invalid = (unsigned long)(~0ul);
        while(begin<end)
        {
            unsigned half = (end-begin) >> 1;
            const char* ptr = (const char*) &names[ data[(begin+half)*2] ];
            int c = comparator(ptr, key, key_length);
            if(!c)
            {
                c = ptr[key_length];
                if(c == '\0')
                {
                    //fprintf(stderr, "Comparing(%s) and (%.*s) = %d\n", ptr,key_length,key, c);
                    return data[(begin+half)*2+1];
                }
            }
            //fprintf(stderr, "Comparing(%s) and (%.*s) = %d\n", ptr,key_length,key, c);
            if(c < 0) begin += half+1;
            else      end   = begin+half;
        }
        return invalid;
    }

    static const unsigned long& BinarySearch(const LongVecType& data, const CharVecType& names, const char* key)
    {
        return BinarySearch(data, names, 0, data.size()/2, key, strlen(key), strncmp);
    }
    struct option;
    struct state;

    typedef unsigned short OptIndexType;
    typedef unsigned short StateIndexType;
    typedef unsigned short StrTableIndexType;
    typedef unsigned short StrTableLengthType;
public:

    CharVecType all_strings;
    LongVecType string_tables;
    CharVecType all_options; // raw storage for struct option[]
    LongVecType all_states;  // namepointer->statepointer
    const state* first_state;

    void Parse(FILE* fp)
    {
        fprintf(stdout, "Parsing syntax file... "); fflush(stdout);

        Clear();

        CharVecType colornames;
        LongVecType colordata;
        bool colors_sorted = false;
        unsigned options_merged = 0, state_bytes = 0;
        all_options.reserve(256*sizeof(option));
        all_states.reserve(256*2);
        colornames.reserve(16384);
        colordata.reserve(256);

        struct state* current_state = nullptr;

        char Buf[512] = {0};
        while(fgets(Buf, sizeof(Buf), fp))
        {
            //fprintf(stderr, "Processing line: %s", Buf);
            cleanup(Buf);
            char* line = Buf+1;
            switch(Buf[0])
            {
                case '=':
                {
                    // Parse color declaration
                    while(*line==' '||*line=='\t') ++line;
                    char* namebegin = line;
                    while(*line && *line != ' ' && *line!='\t') ++line;
                    char* nameend = line;
                    while(*line==' '||*line=='\t') ++line;
                    *nameend = '\0';
                    unsigned long data[2] = {AddString(colornames, namebegin, nameend), ParseColorDeclaration(line)};
                    colordata.insert(colordata.end(), data, data+2);
                    colors_sorted = false;
                    break;
                }
                case ':':
                {
                    if(!colors_sorted)
                    {
                        /* Sort the color table when the first state is encountered */
                        sort_info    = (const char*) &colornames[0];
                        sort_igncase = 0;
                        qsort(&colordata[0], colordata.size()/2, sizeof(colordata[0])*2, ItemCompare);
                        colors_sorted = true;
                        //for(unsigned a=0; a<colordata.size(); a+=2)
                        //    fprintf(stderr, "Color '%s': 0x%lX\n", &colornames[colordata[a]], colordata[a+1]);
                    }
                    // Parse state declaration
                    char* line = Buf+1;
                    while(*line==' '||*line=='\t') ++line;
                    char* namebegin = line;
                    while(*line && *line != ' ' && *line!='\t') ++line;
                    char* nameend = line;
                    while(*line==' '||*line=='\t') ++line;
                    *nameend = '\0';
                    unsigned long name_ptr = AddString(colornames, namebegin, nameend);

                    struct state* s = new state;
                    memset(s->options, 255, sizeof(s->options));

                    // Identify the color. Use binary search
                    const char* colorname = line;
                    unsigned long attr = BinarySearch(colordata, colornames, colorname);
                    s->attr = attr;
                    if(attr == (unsigned long)(~0ul))
                    {
                        s->attr = MakeJSFerrorColor('\0');
                        fprintf(stderr, "Unknown color: '%s'\n", colorname);
                    }
                #ifdef JSF_FORGO_SAVINGS
                    s->name      = namebegin;
                    s->colorname = colorname;
                #endif
                    current_state          = s;
                #ifndef __BORLANDC__
                    static_assert(sizeof(unsigned long) >= sizeof(s), "unsigned long is too small for pointers");
                #endif
                    unsigned long data[2] = {name_ptr, (unsigned long)s};
                    all_states.insert(all_states.end(), data, data+2);
                    state_bytes += sizeof(*s);
                    break;
                }
                case ' ': case '\t':
                {
                    unsigned char bitmask[256/8] = {0};
                    memset(bitmask, 0, sizeof(bitmask));
                    struct option o = ParseStateLine(Buf, fp, bitmask, colornames);

                    unsigned long option_addr = 0;
                    // Reuse duplicate slot from all_options[] if possible
                  #ifndef __DJGPPq__
                    for(; option_addr < all_options.size(); option_addr += sizeof(o))
                        if(memcmp(&all_options[option_addr], &o, sizeof(o)) == 0)
                            break;
                  #else
                    option_addr = all_options.size();
                  #endif

                    if(option_addr == all_options.size())
                        all_options.insert(all_options.end(), (const unsigned char*)&o, (const unsigned char*)&o + sizeof(o));
                    else
                        options_merged += 1;

                    option_addr /= sizeof(o);

                    if( option_addr > OptIndexType(~0ul))
                    {
                        fprintf(stderr, "Too many distinct options in JSF rules\n");
                    }

                    for(unsigned n=0; n<256; ++n)
                        if(bitmask[n/8] & (1u << (n%8u)))
                        {
                        #ifdef JSF_FORGO_SAVINGS
                            fprintf(stderr, "State[%s]Char %02X will point to option %lu (state %s)\n",
                                current_state->name.c_str(),
                                n,
                                option_addr,
                                &colornames[o.state_ptr]);
                        #endif
                            current_state->options[n] = option_addr;
                        }

                    break;
                }
            }
        }
        fprintf(stdout, "Binding... "); fflush(stdout);
        colordata.clear();

        // This is BindStates().
        // Sort the state names for quick lookup
        first_state = (const state*) all_states[1]; // Read the first state pointer before the array is sorted.
        sort_info    = (const char*) &colornames[0];
        sort_igncase = 0;
        qsort(&all_states[0], all_states.size()/2, sizeof(all_states[0])*2, ItemCompare);

      #ifdef JSF_FORGO_SAVINGS
        for(unsigned ptr=0; ptr<all_states.size(); ptr+=2)
            fprintf(stderr, "State %u: Name pointer = '%s', state name = '%s'\n",
                ptr/2,
                &colornames[all_states[ptr+0]],
                ((state*)all_states[ptr+1])->name.c_str());
      #endif

        // Process all options, and change their state_ptr into an actual pointer.
        {for(unsigned ptr=0; ptr<all_options.size(); ptr+=sizeof(struct option))
        {
            struct option& o = *(struct option*)&all_options[ptr];
            const char* name = (const char*) &colornames[o.state_ptr];
            const unsigned long& v = BinarySearch(all_states, colornames, name);
            if(v == (unsigned long)(~0ul)) fprintf(stderr, "Unknown state name: %s\n", name);
            else
            {
                unsigned stateindex = (&v - &all_states[0]) / 2;
            #ifdef JSF_FORGO_SAVINGS
                fprintf(stderr, "Option %zu: State %s will be state 0x%lu(%s), i.e. index %u\n",
                    ptr/sizeof(struct option), name,
                    v,
                    ((struct state*)v)->name.c_str(),
                    stateindex);
            #endif

                if(stateindex > StateIndexType(~0ul)) fprintf(stderr, "StateIndexType is too small.\n");
                o.state_ptr = stateindex;
            }
        }}
        // The process all string tables, and change their state_ptr into an actual pointer.
        {for(unsigned ptr=0; ptr<string_tables.size(); ptr+=2)
        {
            const char* name = (const char*) &colornames[ string_tables[ptr+1] ];
            string_tables[ptr+1] = BinarySearch(all_states, colornames, name);
            if(string_tables[ptr+1] == (unsigned long)(~0ul)) fprintf(stderr, "Unknown state name: %s\n", name);
        }}
        // Then reduce the states array into just the pointers
        {for(unsigned ptr=0; ptr<all_states.size(); ptr+=2)
        {
            const char* name = (const char*) &colornames[ all_states[ptr+0] ];
            const struct state* s = (const struct state*)all_states[ptr+1];
            for(unsigned n=0; n<256; ++n)
                if(s->options[n] == OptIndexType(~0ul))
                    fprintf(stderr, "State %s: Character %u still undefined!\n", name, n);

            all_states[ptr/2] = all_states[ptr+1];
        }}
        all_states.resize(all_states.size()/2);

        fprintf(stdout, "Done; strings=%lu, strtab=%lu, opt=%lu, states=%lu; %u merged opts; %u state bytes\n",
            (unsigned long) (all_strings.size()),
            (unsigned long) (string_tables.size()),
            (unsigned long) (all_options.size()/sizeof(option)),
            (unsigned long) (all_states.size()/sizeof(void*)),
            options_merged, state_bytes);
        fflush(stdout);
        // colornames & colordata will be freed automatically due to scope.
        colornames.clear();
        all_strings.shrink_to_fit();
        string_tables.shrink_to_fit();
        all_options.shrink_to_fit();
        all_states.shrink_to_fit();
    }
    struct ApplyState
    {
        /* std::vector<unsigned char> */
        CharVecType buffer;
        bool        buffering;
        int         recolor, markbegin, markend;
        bool        recolormark, noeat;
        unsigned char c;
        const state*  s;
    };
    void ApplyInit(ApplyState& state)
    {
        state.buffer.clear();
        state.buffering = state.noeat = false;
        state.recolor = state.markbegin = state.markend = 0;
        state.c = '?';
        state.s = first_state;
    }
#if defined(__cplusplus) && __cplusplus >= 199700L
    void Apply( ApplyState& state )
#else
    struct Applier
    {
        virtual cdecl int Get(void) = 0;
        virtual cdecl void Recolor(register unsigned distance, register unsigned n, register EditorCharType attr) = 0;
    };
    void Apply( ApplyState& state, Applier& app)
#endif
    {
#if defined(__cplusplus) && __cplusplus >= 199700L
        DerivedClass& app = *this;
#endif
        for(;;)
        {
        #ifdef JSF_FORGO_SAVINGS
            fprintf(stdout, "[State %s]", state.s->name.c_str());
        #endif
            if(state.noeat)
            {
                state.noeat = false;
                if(!state.recolor) state.recolor = 1;
            }
            else
            {
                int ch = app.Get();
                if(ch < 0) break;
                state.c       = ch;
                state.recolor += 1;
                ++state.markbegin;
                ++state.markend;
            }
            if(state.recolor)
            {
                app.Recolor(0, state.recolor, state.s->attr);
            }
            if(state.recolormark)
            {
                // markbegin & markend say how many characters AGO it was marked
                app.Recolor(state.markend+1, state.markbegin - state.markend, state.s->attr);
            }

            unsigned long opt_ptr = sizeof(option) * state.s->options[state.c];
            const struct option& o = *(const struct option*) &all_options[opt_ptr];
            state.recolor     = o.recolor;
            state.recolormark = o.recolormark;
            state.noeat       = o.noeat;
            state.s           = (const struct state*) all_states[o.state_ptr];
            if(o.strings)
            {
                const char* k = (const char*) &state.buffer[0];
                unsigned    n = state.buffer.size();
                unsigned long ns = BinarySearch(
                    string_tables, all_strings,
                    o.string_table_begin, o.string_table_begin + o.string_table_length,
                    k,n,
                    o.strings==1 ? strncmp : strncasecmp);
        #ifdef JSF_FORGO_SAVINGS
                fprintf(stdout, "String table: Tried '%.*s', got 0x%lX (%s)", n,k, ns,
                    ns != (unsigned long)(~0ul) ? ((const struct state*)ns)->name.c_str() : "(null)"
                );
        #endif
                if(ns != (unsigned long)(~0ul))
                {
                    state.s       = (const struct state*) ns;
                    state.recolor = state.buffer.size()+1;
                }
                state.buffer.clear();
                state.buffering = false;
            }
            else if(state.buffering && !state.noeat)
            {
        #ifdef JSF_FORGO_SAVINGS
                fprintf(stdout, "Buffering char %02X\n", state.c);
        #endif
                state.buffer.push_back(state.c);
            }
            else
            {
        #ifdef JSF_FORGO_SAVINGS
                fprintf(stdout, "Based on char %02X (%c), choosing option %lu -- state %s (recolor=%d, mark=%d, noeat=%d)\n",
                    state.c, state.c, opt_ptr,
                    state.s->name.c_str(),
                    state.recolor, state.recolormark, state.noeat);
        #endif
            }
            if(o.buffer)
                { state.buffering = true;
                  state.buffer.assign(&state.c, &state.c + 1); }
            if(o.mark)    { state.markbegin = 0; }
            if(o.markend) { state.markend   = 0; }
        }
    }
private:
    inline static unsigned long ParseColorDeclaration(char* line)
    {
        unsigned char fg256 = 0;
        unsigned char bg256 = 0;
        unsigned char flags = 0x00; // underline=1 dim=2 italic=4 bold=8 inverse=16 blink=32
        for(;;)
        {
            while(*line==' '||*line=='\t') ++line;
            if(!*line) break;
            {char* line_end = nullptr;
            {unsigned char val = strtol(line, &line_end, 16);
            if(line_end >= line+2) // Two-digit hex?
            {
                line     = line_end;
                fg256    = val & 0xF;
                bg256    = val; bg256 >>= 4;
                continue;
            }}
            if(line[1] == 'g' && line[2] == '_' && line[3] >= '0' && line[3] <= '9')
            {
                unsigned base = (line[5] >= '0' && line[5] <= '5') ? (16+(6<<8)) : (232+(10<<8));
                unsigned char val = (unsigned char)(base) + strtol(line+3, &line_end, base>>8u);
                switch(line[0]) { case 'b': bg256 = val; break;
                                  case 'f': fg256 = val; break; }
                line = line_end; continue;
            }}
            /* Words: black blue cyan green red yellow magenta white
             *        BLACK BLUE CYAN GREEN RED YELLOW MAGENTA WHITE
             *        bg_black bg_blue bg_cyan bg_green bg_red bg_yellow bg_magenta bg_white
             *        BG_BLACK BG_BLUE BG_CYAN BG_GREEN BG_RED BG_YELLOW BG_MAGENTA BG_WHITE
             *        underline=1 dim=2 italic=4 bold=8 inverse=16 blink=32
             *
             * This hash has been generated using find_jsf_formula.cc.
             * As long as it differentiates the *known* words properly
             * it does not matter what it does to unknown words.
             */
            unsigned short c=0, i=0;
//Good: 90 28   distance = 46  mod=46  div=26
            while(*line && *line != ' ' && *line != '\t') { c += 90u*(unsigned char)*line + i; i+=28; ++line; }
            unsigned char code = ((c + 22u) / 26u) % 46u;

#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
            static const signed char actions[46] = { 10,29,2,4,31,22,27,23,11,36,15,28,7,6,25,-1,17,-1,24,12,16,30,-1,8,35,0,9,19,-1,3,14,20,21,33,32,34,1,13,-1,-1,5,26,-1,-1,18,37};
#endif
#ifdef ATTRIBUTE_CODES_IN_VGA_ORDER
            static const signed char actions[46] = { 10,30,2,1,31,19,29,23,13,36,15,25,7,3,28,-1,20,-1,24,9,16,27,-1,8,35,0,12,21,-1,5,11,17,22,33,32,34,4,14,-1,-1,6,26,-1,-1,18,37};
#endif
            /*if(code >= 0 && code <= 45)*/ code = actions[code - 0];
            switch(code >> 4) { case 0: fg256 = code&15; break;
                                case 1: bg256 = code&15; break;
                                default:flags |= 1u << (code&15); }
        }
        // Type is unsigned long to avoid compiler warnings from pointer cast
        unsigned long attr = ComposeEditorChar('\0', fg256, bg256, flags);
        if(!attr) attr |= 0x80000000ul; // set 1 dummy bit in order to differentiate from nuls
        return attr;
    }

    struct option;
    struct state
    {
        // Each of the possible 256 followups is an option.
        OptIndexType   options[256]; // Index to all_options[].
        EditorCharType attr;

    #ifdef JSF_FORGO_SAVINGS
        std::string name, colorname;
    #endif

        state(): options(),attr() {}
    };
    // An option is a line in JSF file under ':', for example "idle noeat" is an option.
    struct option
    {
        // Index into string_tables[]
        StrTableIndexType  string_table_begin;
        // Before BindStates(), state_ptr is an index to colornames[].
        // After BindStates(),  state_ptr is an index to all_states[].
        StateIndexType     state_ptr;
        StrTableLengthType string_table_length;
        //
        unsigned char recolor;
        bool          noeat:  1;
        bool          buffer: 1;
        unsigned char strings:2; // 0=no strings, 1=strings, 2=istrings
        bool          mark:1, markend:1, recolormark:1;
    };
    struct option ParseStateLine(char* line, FILE* fp, unsigned char bitmask[256/8],
                                 CharVecType& colornames)
    {
        struct option o = {};
        memset(&o, 0, sizeof(o));

        while(*line == ' ' || *line == '\t') ++line;
        if(*line == '*')
        {
            memset(bitmask, 0xFF, 256/8); // All characters refer to this option
            ++line;
        }
        else if(*line == '"')
        {
            for(++line; *line != '\0' && *line != '"'; ++line)
            {
                if(*line == '\\')
                    switch(*++line)
                    {
                        case 't': *line = '\t'; break;
                        case 'n': *line = '\n'; break;
                        case 'v': *line = '\v'; break;
                        case 'b': *line = '\b'; break;
                    }
                unsigned char first = *line;
                if(line[1] == '-' && line[2] != '"')
                {
                    line += 2;
                    if(*line == '\\')
                        switch(*++line)
                        {
                            case 't': *line = '\t'; break;
                            case 'n': *line = '\n'; break;
                            case 'v': *line = '\v'; break;
                            case 'b': *line = '\b'; break;
                        }
                    do bitmask[first/8] |= 1u << (first%8u);
                    while(first++ != (unsigned char)*line);
                }
                else
                    bitmask[first/8] |= 1u << (first%8u);
            }
            if(*line == '"') ++line;
        }
        while(*line == ' ' || *line == '\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend   = line;
        while(*line == ' ' || *line == '\t') ++line;
        *nameend = '\0';
        unsigned state_ptr = AddString(colornames, namebegin, nameend);
        if(state_ptr > StateIndexType(~0ul))
            fprintf(stderr, "StateIndexType is too small.\n");
        o.state_ptr = state_ptr;

        while(*line != '\0')
        {
            char* opt_begin = line;
            while(*line && *line != ' ' && *line!='\t') ++line;
            char* opt_end   = line;
            while(*line == ' ' || *line == '\t') ++line;
            *opt_end = '\0';

            /* Words: noeat buffer markend mark strings istrings recolormark recolor=
             * This hash has been generated using jsf-keyword-hash2.php.
             */
            register unsigned char n=2;
            {for(register unsigned char v=0;;)
            {
                char c = *opt_begin++;
                n += (c ^ v) + 6;
                v += 2;
                if(c == '=' || c == '\0') break;
            }}
            switch((n >> 3u) & 7)
            {
                case 0: o.recolormark = true; break; // recolormark
                case 1: o.noeat       = true; break; // noeat
                case 3: o.mark        = true; break; // mark
                case 4: o.strings     = 1;    break; // strings
                case 5: o.markend     = true; break; // markend
                case 6: o.strings     = 2;    break; // istrings
                case 7: o.buffer      = true; break; // buffer
                //default:fprintf(stdout,"Unknown keyword '%s' in '%s'\n", opt_begin-1, namebegin); break;
                case 2: int r = atoi(opt_begin);// recolor=
                        if(r < 0) r = -r;
                        o.recolor = r;
                        break;
            }
        }
        if(o.strings)
        {
            if(string_tables.size()/2 > StrTableIndexType(~0ul))
                fprintf(stderr, "StrTableIndexType is too small.\n");
            o.string_table_begin = string_tables.size()/2;
            for(;;)
            {
                char Buf[512]={0};
                if(!fgets(Buf, sizeof(Buf), fp)) break;
                cleanup(Buf);
                line = Buf;
                while(*line == ' ' || *line == '\t') ++line;
                if(strcmp(line, "done") == 0) break;
                if(*line == '"') ++line;

                char* key_begin = line;
                while(*line != '"' && *line != '\0') ++line;
                char* key_end   = line;
                if(*line == '"') ++line;
                while(*line == ' ' || *line == '\t') ++line;
                *key_end     = '\0';

                char* value_begin = line;
                while(*line != '\0') ++line;
                char* value_end   = line;
                if(*key_begin && *value_begin)
                {
                    unsigned long string_address = AddString(all_strings, key_begin,  key_end);
                    unsigned long value_address  = AddString(colornames, value_begin, value_end);
                    unsigned long data[2] = {string_address, value_address};
                    string_tables.insert(string_tables.end(), data, data+2);
                }
            }
            unsigned length = (string_tables.size()/2 - o.string_table_begin);
            if(length > StrTableLengthType(~0ul))
                fprintf(stderr, "StrTableLengthType is too small.\n");

            o.string_table_length = length;
            sort_info    = (const char*) &all_strings[0];
            sort_igncase = o.strings == 2;
            qsort(&string_tables[o.string_table_begin*2], o.string_table_length,
                  sizeof(&string_tables[0])*2, ItemCompare);
            /*
            for(unsigned n=0; n<length; ++n)
                fprintf(stderr, "%s: %s\n", &all_strings[string_tables[(o.string_table_begin+n)*2+0]],
                                            &colornames[string_tables[(o.string_table_begin+n)*2+1]]);*/
        }
        return o;
    }

public:
    void Clear()
    {
        for(unsigned ptr=0; ptr<all_states.size(); ++ptr)
            delete (state*)all_states[ptr];

        string_tables.clear();
        all_strings.clear();
        all_options.clear();
        all_states.clear();
        first_state = nullptr;
    }
    JSF(const JSF& b) { operator=(b); }
    JSF& operator=(const JSF& b)
    {
        if(this != &b)
        {
            all_states.clear();
            all_strings   = b.all_strings;
            string_tables = b.string_tables;
            all_options   = b.all_options;
            first_state   = b.first_state;
            for(unsigned ptr=0; ptr<b.all_states.size(); ++ptr)
                all_states.push_back((unsigned long)new state(*(const state*)b.all_states[ptr]));
        }
        return *this;
    }
#if defined(__cplusplus) && __cplusplus >= 201100L
    JSF& operator=(JSF&& b)
    {
        if(this != &b)
        {
            all_strings = std::move(b.all_strings);
            string_tables = std::move(b.string_tables);
            all_options = std::move(b.all_options);
            all_states = std::move(b.all_states);
            first_state = std::move(b.first_state);
        }
        return *this;
    }
    JSF(JSF&& b) { operator=(std::move(b)); }
#endif
};
