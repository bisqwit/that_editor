/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include <string.h>

class JSF
{
public:
    JSF() : states(0)
    {
    }
    void Parse(const char* fn)
    {
        FILE* fp = fopen(fn, "rb");
        if(!fp) { perror(fn); return; }
        Parse(fp);
        fclose(fp);
    }
    void Parse(FILE* fp)
    {
        char Buf[512]={0};
        fprintf(stdout, "Parsing syntax file... "); fflush(stdout);
        TabType cc;
        char colors_sorted = 0;
        states = 0; // NOTE: THIS LEAKS MEMORY
        while(fgets(Buf, sizeof(Buf), fp))
        {
            cleanup(Buf);
            if(Buf[0] == '=') { colors_sorted=0; ParseColorDeclaration(Buf+1, cc); }
            else if(Buf[0] == ':')
            {
                if(!colors_sorted)
                {
                    sort(cc);
                    colors_sorted = 1;
                }
                ParseStateStart(Buf+1, cc);
            }
            else if(Buf[0] == ' ' || Buf[0] == '\t')
                ParseStateLine(Buf, fp);
        }
        fprintf(stdout, "Binding... "); fflush(stdout);
        BindStates();

        for(unsigned n=0; n<cc.size(); ++n) free(cc[n].token);

        fprintf(stdout, "Done\n"); fflush(stdout);
    }
    struct state;
    struct ApplyState
    {
        /* std::vector<unsigned char> */
        CharVecType buffer;
        int buffering;
        int recolor, noeat;
        unsigned char c;
        state* s;
    };
    void ApplyInit(ApplyState& state)
    {
        state.buffer.clear();
        state.buffering = state.recolor = state.noeat = 0;
        state.c = '?';
        state.s = states;
    }
    struct Applier
    {
        virtual cdecl int Get(void) = 0;
        virtual cdecl void Recolor(register unsigned n, register unsigned attr) = 0;
    };
    void Apply( ApplyState& state, Applier& app)
    {
        for(;;)
        {
            /*fprintf(stdout, "[State %s]", state.s->name);*/
            if(state.noeat)
            {
                state.noeat = 0;
                if(!state.recolor) state.recolor = 1;
            }
            else
            {
                int ch = app.Get();
                if(ch < 0) break;
                state.c = ch;
                state.recolor += 1;
            }
            app.Recolor(state.recolor, state.s->attr);
            state.recolor = 0;

            option *o = state.s->options[state.c];
            state.recolor = o->recolor;
            state.noeat   = o->noeat;
            state.s = o->state;
            if(o->strings)
            {
                const char* k = (const char*) &state.buffer[0];
                unsigned    n = state.buffer.size();
                struct state* ns = o->strings==1
                        ? findstate(o->stringtable, k, n)
                        : findstate_i(o->stringtable, k, n);
                /*fprintf(stdout, "Tried '%.*s' for %p (%s)\n",
                    n,k, ns, ns->name);*/
                if(ns)
                {
                    state.s = ns;
                    state.recolor = state.buffer.size()+1;
                }
                state.buffer.clear();
                state.buffering = 0;
            }
            else if(state.buffering && !state.noeat)
                state.buffer.push_back(state.c);
            if(o->buffer)
                { state.buffering = 1;
                  state.buffer.assign(&state.c, &state.c + 1); }
        }
    }
private:
    struct option;
    struct state
    {
        state*   next;
        char*   name;
        int     attr;
        option* options[256];
    }* states;
    struct table_item
    {
        char*  token;
        union
        {
            struct state* state;
            char*  state_name;
        };

        inline void Construct() { token=0; state=0; }
        inline void Construct(const table_item& b) { token=b.token; state=b.state; }
        inline void Destruct() { }
        inline void swap(table_item& b)
        {
            register char* t;
            t = token;      token     =b.token;      b.token     =t;
            t = state_name; state_name=b.state_name; b.state_name=t;
        }
    };
    /* std::vector<table_item> without STL, for Borland C++ */
    #define UsePlacementNew
    #define T       table_item
    #define VecType TabType
    #include "vecbase.hh"
    #undef TabType
    #undef T
    #undef UsePlacementNew

    struct option
    {
        TabType stringtable;
        union
        {
            struct state* state;
            char*  state_name;
        };
        unsigned char recolor;
        unsigned noeat:  1;
        unsigned buffer: 1;
        unsigned strings:2; // 0=no strings, 1=strings, 2=istrings
        unsigned name_mapped:1; // whether state(1) or state_name(0) is valid
    };
    inline void ParseColorDeclaration(char* line, TabType& colortable)
    {
        while(*line==' '||*line=='\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend = line;
        while(*line==' '||*line=='\t') ++line;
        int attr = strtol(line, 0, 16);
        *nameend = '\0';
        table_item tmp;
        tmp.token = strdup(namebegin);
        if(!tmp.token) fprintf(stdout, "strdup: failed to allocate string for %s\n", namebegin);
        tmp.state = (struct state*) attr;
        colortable.push_back(tmp);
    }
    inline void ParseStateStart(char* line, const TabType& colortable)
    {
        while(*line==' '||*line=='\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend = line;
        while(*line==' '||*line=='\t') ++line;
        *nameend = '\0';
        struct state* s = new state;
        if(!s) fprintf(stdout, "failed to allocate new jsf state\n");
        memset(s, 0, sizeof(*s));
        s->name = strdup(namebegin);
        if(!s->name) fprintf(stdout, "strdup: failed to allocate string for %s\n", namebegin);
        {state* c = findstate(colortable, line);
        if(!c) { s->attr = 0x4A; fprintf(stdout,"Unknown color: '%s'\n", line); }
        else   s->attr = (long) c;}
        s->next = states;
        states = s;
    }
    inline void ParseStateLine(char* line, FILE* fp)
    {
        option* o = new option;
        if(!o) fprintf(stdout, "failed to allocate new jsf option\n");
        memset(o, 0, sizeof(*o));
        while(*line == ' ' || *line == '\t') ++line;
        if(*line == '*')
        {
            for(unsigned a=0; a<256; ++a)
                states->options[a] = o;
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
                    do states->options[first] = o;
                    while(first++ != (unsigned char)*line);
                }
                else
                    states->options[first] = o;
            }
            if(*line == '"') ++line;
        }
        while(*line == ' ' || *line == '\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend   = line;
        while(*line == ' ' || *line == '\t') ++line;
        *nameend = '\0';
        o->state_name  = strdup(namebegin);
        if(!o->state_name) fprintf(stdout, "strdup: failed to allocate string for %s\n", namebegin);
        o->name_mapped = 0;
        /*fprintf(stdout, "'%s' for these: ", o->state_name);
        for(unsigned c=0; c<256; ++c)
            if(states->options[c] == o)
                fprintf(stdout, "%c", c);
        fprintf(stdout, "\n");*/

        while(*line != '\0')
        {
            char* opt_begin = line;
            while(*line && *line != ' ' && *line!='\t') ++line;
            char* opt_end   = line;
            while(*line == ' ' || *line == '\t') ++line;
            *opt_end = '\0';
            switch(*opt_begin)
            {
                case 'n':
                    if(strcmp(opt_begin+1, /*"n"*/"oeat") == 0) { o->noeat = 1; break; }
                case 'b':
                    if(strcmp(opt_begin, "buffer") == 0) { o->buffer = 1; break; }
                case 's':
                    if(strcmp(opt_begin, "strings") == 0) { o->strings = 1; break; }
                case 'i':
                    if(strcmp(opt_begin, "istrings") == 0) { o->strings = 2; break; }
                case 'r':
                    if(strncmp(opt_begin, "recolor=", 8) == 0)
                    {
                        int r = atoi(opt_begin+8);
                        if(r < 0) r = -r;
                        o->recolor = r;
                        break;
                    }
                default:
                    fprintf(stdout,"Unknown keyword '%s' in '%s'\n", opt_begin, namebegin);
            }
        }
        if(o->strings)
        {
            for(;;)
            {
                char Buf[512]={0};
                if(!fgets(Buf, sizeof(Buf), fp)) break;
                cleanup(Buf);
                line = Buf;
                while(*line == ' ' || *line == '\t') ++line;
                if(strcmp(line, "done") == 0) break;
                if(*line == '"') ++line;

                char* key_begin = line = strdup(line);
                if(!key_begin) fprintf(stdout, "strdup: failed to allocate string for %s\n", line);
                while(*line != '"' && *line != '\0') ++line;
                char* key_end   = line;
                if(*line == '"') ++line;
                while(*line == ' ' || *line == '\t') ++line;
                *key_end++   = '\0';

                char* value_begin = line;
                while(*line != '\0') ++line;
                /*unsigned char* value_end   = (unsigned char*) line;
                *value_end++ = '\0';*/
                table_item item;
                item.token      = key_begin;
                item.state_name = value_begin;
                o->stringtable.push_back(item);
            }
            sort(o->stringtable);
        }
    }
    // Removes comments and trailing space from the buffer
    void cleanup(char* Buf)
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
    // Search given table for the given string.
    // Is used by BindStates() for finding states for binding,
    // but also used by Apply for searching a string table
    // (i.e. used when coloring reserved words).
    static state* findstate(const TabType& table, const char* s, register unsigned n=0)
    {
        if(!n) n = strlen(s);
        unsigned begin = 0, end = table.size();
        while(begin < end)
        {
            unsigned half = (end-begin) >> 1;
            const table_item& m = table[begin + half];
            register int c = strncmp(m.token, s, n);
            if(c == 0)
            {
                if(m.token[n] == '\0') return m.state;
                c = m.token[n];
            }
            if(c < 0) begin += half+1;
            else      end = begin+half;
        }
        return 0;
    }
    // Case-ignorant version
    static state* findstate_i(const TabType& table, const char* s, register unsigned n=0)
    {
        if(!n) n = strlen(s);
        unsigned begin = 0, end = table.size();
        while(begin < end)
        {
            unsigned half = (end-begin) >> 1;
            const table_item& m = table[begin + half];
            register int c = strnicmp(m.token, s, n);
            if(c == 0)
            {
                if(m.token[n] == '\0') return m.state;
                c = m.token[n];
            }
            if(c < 0) begin += half+1;
            else      end = begin+half;
        }
        return 0;
    }

    // Converted state-names into pointers to state structures for fast access
    void BindStates()
    {
        TabType state_cache;
        {for(state* s = states; s; s = s->next)
        {
            table_item tmp;
            tmp.token = s->name;
            tmp.state = s;
            state_cache.push_back(tmp);
        }}
        sort(state_cache);

        // Translate state names to state pointers.
        for(;;)
        {
            for(unsigned a=0; a<256; ++a)
            {
                option* o = states->options[a];
                if(!o)
                {
                    fprintf(stdout, "In state '%s', character state %u/256 not specified\n", states->name, a);
                    continue;
                }
                if( ! o->name_mapped)
                {
                    char* name = o->state_name;
                    o->state = findstate( state_cache, name );
                    if(!o->state)
                    {
                        fprintf(stdout, "Failed to find state called '%s' for index %u/256 in '%s'\n", name, a, states->name);
                    }
                    free(name);
                    o->name_mapped = 1;
                    for(TabType::iterator e = o->stringtable.end(),
                        t = o->stringtable.begin();
                        t != e;
                        ++t)
                    {
                        name = t->state_name;
                        t->state = findstate( state_cache, name );
                        // free(name); - was not separately allocated
            }   }   }
            if(!states->next) break;
            // Get the first-inserted state (last in chain) as starting-point.
            states = states->next;
    }   }

    static int TableItemCompareForSort(const void * a, const void * b)
    {
        table_item * aa = (table_item *)a;
        table_item * bb = (table_item *)b;
        return strcmp(aa->token, bb->token);
    }
    static inline void sort(TabType& tab)
    {
        /*
        // Sort the table using insertion sort
        unsigned b = tab.size();
        for(unsigned i, j=1; j<b; ++j)
        {
            table_item k = tab[j];
            for(i=j; i>=1 && strcmp( k.token, tab[i-1].token ) > 0; ++i)
                tab[i] = tab[i-1];
            tab[i] = k;
        }
        */
        qsort(&tab[0], tab.size(), sizeof(tab[0]), TableItemCompareForSort);
    }
};
