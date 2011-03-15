#include <string.h>

class JSF
{
public:
    JSF() : colors(0), states(0)
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
        fprintf(stdout, "Parsing syntax file... "); fflush(stderr);
        while(fgets(Buf, sizeof(Buf), fp))
        {
            cleanup(Buf);
            if(Buf[0] == '=') ParseColorDeclaration(Buf+1);
            else if(Buf[0] == ':') ParseStateStart(Buf+1);
            else if((Buf[0] == ' ' || Buf[0] == '\t')
                 || strcmp(Buf, "done") == 0)
                ParseStateLine(Buf, fp);
        }
        fprintf(stdout, "Binding... "); fflush(stderr);
        BindStates();
        while(colors)
        {
            color* c = colors->next;
            free(colors->name);
            delete colors; colors = c;
        }
        fprintf(stdout,"Done\n"); fflush(stderr);
    }
    struct state;
    struct ApplyState
    {
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
                        ? findstate(o->stringtable, o->numstrings, k, n)
                        : findstate_i(o->stringtable, o->numstrings, k, n);
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
    struct color { struct color* next; char* name; int attr; }* colors;
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
        struct state* state;
        char*  state_name;
    };
    static cdecl int TableItemCompareForSort(const void * a, const void * b)
    {
        table_item * aa = (table_item *)a;
        table_item * bb = (table_item *)b;
        return strcmp(aa->token, bb->token);
    }
    struct option
    {
        struct state* state;
        char*  state_name;
        table_item* stringtable;
        unsigned      numstrings;
        unsigned char recolor;
        unsigned noeat:  1;
        unsigned buffer: 1;
        unsigned strings:2; // 0=no strings, 1=strings, 2=istrings
    };
    void ParseColorDeclaration(char* line)
    {
        while(*line==' '||*line=='\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend = line;
        while(*line==' '||*line=='\t') ++line;
        int attr = strtol(line, 0, 16);
        *nameend = '\0';
        color* n = new color;
        n->name = strdup(namebegin);
        n->attr = attr;
        n->next = colors;
        //fprintf(stdout, "'%s' is 0x%02X\n", n->name, n->attr);
        colors = n;
    }
    void ParseStateStart(char* line)
    {
        while(*line==' '||*line=='\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend = line;
        while(*line==' '||*line=='\t') ++line;
        *nameend = '\0';
        struct state* s = new state;
        memset(s, 0, sizeof(*s));
        s->name = strdup(namebegin);
        color* c = findcolor(line);
        if(!c) fprintf(stdout,"Unknown color: '%s'\n", line);
        s->attr = c ? c->attr : 0x4A;
        s->next = states;
        states = s;
    }
    void ParseStateLine(char* line, FILE* fp)
    {
        state*  s = states;
        option* o = new option;
        memset(o, 0, sizeof(*o));
        while(*line == ' ' || *line == '\t') ++line;
        if(*line == '*')
        {
            for(unsigned a=0; a<256; ++a)
                s->options[a] = o;
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
                    do s->options[first] = o;
                    while(first++ != (unsigned char)*line);
                }
                else
                    s->options[first] = o;
            }
            if(*line == '"') ++line;
        }
        while(*line == ' ' || *line == '\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend   = line;
        while(*line == ' ' || *line == '\t') ++line;
        *nameend = '\0';
        o->state_name = strdup(namebegin);
        /*fprintf(stdout, "'%s' for these: ", o->state_name);
        for(unsigned c=0; c<256; ++c)
            if(s->options[c] == o)
                fprintf(stdout, "%c", c);
        fprintf(stdout, "\n");*/

        while(*line != '\0')
        {
            char* opt_begin = line;
            while(*line && *line != ' ' && *line!='\t') ++line;
            char* opt_end   = line;
            while(*line == ' ' || *line == '\t') ++line;
            *opt_end = '\0';
            if(strcmp(opt_begin, "noeat") == 0)
                o->noeat = 1;
            else if(strcmp(opt_begin, "buffer") == 0)
                o->buffer = 1;
            else if(strcmp(opt_begin, "strings") == 0)
                o->strings = 1;
            else if(strcmp(opt_begin, "istrings") == 0)
                o->strings = 2;
            else if(strncmp(opt_begin, "recolor=", 8) == 0)
            {
                int r = atoi(opt_begin+8);
                if(r < 0) r = -r;
                o->recolor = r;
            }
            else
                fprintf(stdout,"Unknown keyword '%s' in '%s'\n", opt_begin, namebegin);
        }
        if(o->strings)
        {
            CharVecType table_keys;
            CharVecType table_targets;
            unsigned num_strings = 0;
            for(;;)
            {
                char Buf[512]={0};
                if(!fgets(Buf, sizeof(Buf), fp)) break;
                cleanup(Buf);
                line = Buf;
                while(*line == ' ' || *line == '\t') ++line;
                if(strcmp(line, "done") == 0) break;
                if(*line == '"') ++line;
                while(*line != '"' && *line != '\0')
                    table_keys.push_back(*line++);
                table_keys.push_back('\0');
                if(*line == '"') ++line;
                while(*line == ' ' || *line == '\t') ++line;
                //fprintf(stdout, "[%s]", line);
                while(*line != '\0')
                    table_targets.push_back(*line++);
                table_targets.push_back('\0');

                //fprintf(stdout, "table[%u]='%u:%s' => '%u:%s'\n", num_strings,
                //    a,&table_keys[a], b,&table_targets[b]);
                ++num_strings;
            }
            o->numstrings = num_strings;
            o->stringtable = new table_item[ num_strings ];
            memset(o->stringtable, 0, num_strings*sizeof(*o->stringtable));
            char * k = (char *) &table_keys[0];
            char * v = (char *) &table_targets[0];
            for(unsigned n=0; n<num_strings; ++n)
            {
                //fprintf(stdout, "table[%u]='%s' => '%s'\n", n,k,v);
                o->stringtable[n].token      = strdup(k);
                o->stringtable[n].state_name = strdup(v);
                o->stringtable[n].state = 0;
                k = strchr(k, '\0') + 1;
                v = strchr(v, '\0') + 1;
            }
            qsort(o->stringtable, num_strings, sizeof(*o->stringtable),
                  TableItemCompareForSort);
        }
    }
    // Removes comments and trailing space from the buffer
    void cleanup(char* Buf)
    {
        char* end = strchr(Buf, '\0');
        char* begin = Buf; int quote=0;
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
    static state* findstate
        (table_item* table, unsigned table_size, const char* s, unsigned n=0)
    {
        if(!n) n = strlen(s);
        while(table_size > 0)
        {
            unsigned half = table_size >> 1;
            table_item* m = table + half;
            int c = strncmp(m->token, s, n);
            //fprintf(stdout, "strncmp('%s','%.*s',%u)=%d\n",m->token,n,s,n);
            if(c == 0)
            {
                if(m->token[n] == '\0') return m->state;
                c = m->token[n];
            }
            if(c < 0) { table = m+1; table_size -= half+1; }
            else      { table_size = half; }
        }
        return 0;
    }
    // Case-ignorant version
    static state* findstate_i
        (table_item* table, unsigned table_size, const char* s, unsigned n=0)
    {
        if(!n) n = strlen(s);
        while(table_size > 0)
        {
            unsigned half = table_size >> 1;
            table_item* m = table + half;
            int c = strnicmp(m->token, s, n);
            //fprintf(stdout, "strnicmp('%s','%.*s',%u)=%d\n",m->token,n,s,n);
            if(c == 0)
            {
                if(m->token[n] == '\0') return m->state;
                c = m->token[n];
            }
            if(c < 0) { table = m+1; table_size -= half+1; }
            else      { table_size = half; }
        }
        return 0;
    }
    // Find color-struct by name
    color* findcolor(const char* name) const
    {
        color* c = colors;
        for(; c && strcmp(c->name, name) != 0; c = c->next) { }
        return c;
    }

    // Converted state-names into pointers to state structures for fast access
    void BindStates()
    {
        unsigned num_states = 0;
        {for(state* s = states; s; s = s->next)
            ++num_states;}
        table_item* state_cache = new table_item[ num_states ];
        {unsigned n=0;
        for(state* s = states; s; s = s->next, ++n)
        {
            //fprintf(stdout, "state[%u]='%s'\n", n, s->name);
            state_cache[n].token = s->name;
            state_cache[n].state = s;

            /*fprintf(stdout, "In state %s:\n", s->name);
            for(unsigned c=0; c<256; ++c)
                fprintf(stdout, "  '%c' -> %s\n",
                    c, s->options[c]->state_name);
            fprintf(stdout, "\n");*/
        }}
        qsort(state_cache, num_states, sizeof(*state_cache),
              TableItemCompareForSort);

        for(state* s = states; s; s = s->next)
            for(unsigned a=0; a<256; ++a)
            {
                option* o = s->options[a];
                if( ! o->state )
                {
                    o->state = findstate( state_cache,num_states, o->state_name );
                    /*free(o->state_name);
                    o->state_name = 0;*/
                    unsigned n = o->numstrings;
                    for(unsigned c=0; c<n; ++c)
                    {
                        table_item* t = & (o->stringtable[c]);
                        if( ! t->state )
                        {
                            t->state = findstate( state_cache,num_states, t->state_name );
                            /*free( t->state_name );
                            t->state_name = 0;*/
            }   }   }   }
        delete[] state_cache;
        while(states && states->next)
            states = states->next; // Get the first state as starting-point
    }
};
