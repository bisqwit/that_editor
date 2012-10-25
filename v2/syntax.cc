#include <cstring>
#include <map>
#include <algorithm>
#include <vector>

#include "syntax.hh"

static const std::vector<std::pair<std::string, std::string>>
    JSF_AutoDetections
{
    { "Makefile", "conf.jsf" },
    { ".mak",     "conf.jsf" },
    { ".c",       "c.jsf" },
    { ".h",       "c.jsf" },
    { ".cc",      "c.jsf" },
    { ".hh",      "c.jsf" },
    { ".cpp",     "c.jsf" },
    { "",         "c.jsf" }
};

// Remove comments and trailing space from the buffer
static void cleanup(char* Buf)
{
    char quote=0, *begin = Buf, *end = std::strchr(Buf, '\0');
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

static bool StrEndCompare(const std::string& part, const std::string& full)
{
    return part.size() >= full.size()
        && full.compare(full.size()-part.size(), part.size(),
                        part, 0, part.size()) == 0;
}

void JSF::LoadForFile(const std::string& fn, bool quiet)
{
    for(const auto& l: JSF_AutoDetections)
        if(StrEndCompare(fn, l.first))
        {
            Parse(l.second, quiet);
            return;
        }
}

void JSF::Parse(const std::string& fn, bool quiet)
{
    std::FILE* fp = std::fopen(fn.c_str(), "rb");
    if(!fp) { std::perror(fn.c_str()); return; }
    Parse(fp, quiet);
    std::fclose(fp);
}

void JSF::Parse(std::FILE* fp, bool quiet)
{
    char Buf[512]={0};

    if(!quiet)
    {
        std::fprintf(stdout, "Parsing syntax file... "); std::fflush(stdout);
    }

    std::map<std::string, AttrType> colors;
    std::map<std::string, state*> state_cache;

    states.clear();
    std::list<state::option*> options;

    while(std::fgets(Buf, sizeof(Buf), fp))
    {
        // Remove comments and trailing space from the buffer
        cleanup(Buf);

        // Identify the contents of the buffer
        switch(Buf[0])
        {
            case '=': // Parse color declaration
            {
                char* line = Buf+1;
                while(*line==' '||*line=='\t') ++line;
                char* namebegin = line;
                while(*line && *line != ' ' && *line!='\t') ++line;
                char* nameend = line;
                while(*line==' '||*line=='\t') ++line;
                *nameend = '\0';

                colors[namebegin] = AttrType::ParseJSFattribute(line);
                break;
            }
            case ':': // Parse beginning of state
            {
                // Parse beginning of state
                char* line = Buf+1;
                while(*line==' '||*line=='\t') ++line;
                char* namebegin = line;
                while(*line && *line != ' ' && *line!='\t') ++line;
                char* nameend = line;
                while(*line==' '||*line=='\t') ++line;
                *nameend = '\0';
                // Now namebegin=name, line=colorname

                auto i = colors.find(line);
                AttrType attr = AttrParseError;
                if(i == colors.end())
                {
                    if(!quiet)
                        std::fprintf(stdout, "Unknown color: '%s'\n", line);
                }
                else
                    attr = i->second;

                states.push_back( std::move(state()) );
                state_cache[namebegin] = &states.back();
                break;
            }
            case ' ':
            case '\t': // Parse actual flesh of the state (pattern and definition)
            {
                char* line = Buf;
                while(*line == ' ' || *line == '\t') ++line;

                options.push_front( new state::option );
                auto& s = states.back(); // Current state
                auto& o = *options.front();

                switch(*line)
                {
                    case '*': // Match every character
                        for(unsigned a=0; a<256; ++a)
                            s.options[a] = &o;
                        ++line;
                        break;
                    case '"': // Match characters in this string
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
                                do s.options[first] = &o;
                                while(first++ != (unsigned char)*line);
                            }
                            else
                                s.options[first] = &o;
                        }
                        if(*line == '"') ++line;
                        break;
                }
                while(*line == ' ' || *line == '\t') ++line;
                char* namebegin = line;
                while(*line && *line != ' ' && *line!='\t') ++line;
                char* nameend   = line;
                while(*line == ' ' || *line == '\t') ++line;
                *nameend = '\0';

                o.nameptr = new std::string(namebegin);

                /*fprintf(stdout, "'%s' for these: ", o->state_name);
                for(unsigned c=0; c<256; ++c)
                    if(s.options[c] == o)
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
                            if(strcmp(opt_begin+1, /*"n"*/"oeat") == 0) { o.noeat = 1; break; }
                        case 'b':
                            if(strcmp(opt_begin, "buffer") == 0) { o.buffer = 1; break; }
                        case 's':
                            if(strcmp(opt_begin, "strings") == 0) { o.strings = 1; break; }
                        case 'i':
                            if(strcmp(opt_begin, "istrings") == 0) { o.strings = 2; break; }
                        case 'r':
                            if(strncmp(opt_begin, "recolor=", 8) == 0)
                            {
                                int r = atoi(opt_begin+8);
                                if(r < 0) r = -r;
                                o.recolor = r;
                                break;
                            }
                        default:
                            fprintf(stdout,"Unknown keyword '%s' in '%s'\n", opt_begin, namebegin);
                    }
                }
                if(o.strings)
                {
                    while(std::fgets(Buf, sizeof(Buf), fp))
                    {
                        cleanup(Buf);

                        line = Buf;
                        while(*line == ' ' || *line == '\t') ++line;
                        if(std::strcmp(line, "done") == 0) break;
                        if(*line == '"') ++line;

                        char* key_begin = line;
                        while(*line != '"' && *line != '\0') ++line;
                        char* key_end   = line;
                        if(*line == '"') ++line;
                        while(*line == ' ' || *line == '\t') ++line;
                        *key_end++   = '\0';

                        char* value_begin = line;
                        while(*line != '\0') ++line;
                        /*unsigned char* value_end   = (unsigned char*) line;
                        *value_end++ = '\0';*/

                        state::option::stringopt opt;
                        opt.nameptr = new std::string( value_begin );
                        o.stringtable.push_back( {key_begin, opt} );
                    }
                    o.SortStringTable();
                }
            }
        }
    }

    if(!quiet)
    {
        std::fprintf(stdout, "Binding... "); std::fflush(stdout);
    }

    // Translate state names to state pointers.
    for(auto o: options)
    {
        auto p = state_cache[*o->nameptr];
        delete o->nameptr;
        o->stateptr = p;

        for(auto& t: o->stringtable)
        {
            p = state_cache[*t.second.nameptr];
            delete t.second.nameptr;
            t.second.stateptr = p;
        }
    }

    if(!quiet)
    {
        std::fprintf(stdout, "Done\n"); std::fflush(stdout);
    }
}


void JSF::state::option::SortStringTable()
{
    if(strings==1)
        std::sort(stringtable.begin(), stringtable.end(),
            [](const std::pair<std::string, stringopt>& a,
               const std::pair<std::string, stringopt>& b)
            {
                return a.first < b.first;
            } );
    else
        std::sort(stringtable.begin(), stringtable.end(),
            [](const std::pair<std::string, stringopt>& a,
               const std::pair<std::string, stringopt>& b)
            {
                return strcasecmp(a.first.c_str(), b.first.c_str()) < 0;
            } );
}


std::vector< std::pair<std::string, JSF::state::option::stringopt> >::const_iterator
    JSF::state::option::SearchStringTable(const std::string& s) const
{
    auto i = (strings == 1)
        ? // Case-sensitive matching
          std::lower_bound(stringtable.begin(), stringtable.end(), s,
            [](const std::pair<std::string, stringopt>& a,
               const std::string& s)
            {
                return a.first < s;
            } )
        :  // Case-insensitive
          std::lower_bound(stringtable.begin(), stringtable.end(), s,
            [](const std::pair<std::string, stringopt>& a,
               const std::string& s)
            {
                return strcasecmp(a.first.c_str(), s.c_str()) < 0;
            } );
    if(i == stringtable.end() || i->first != s)
        return stringtable.end();
    return i;
}

JSF::state::~state()
{
    std::sort(std::begin(options), std::end(options));
    option* prev = nullptr;
    for(auto o: options)
        if(o != prev)
            { delete o; prev = o; }
}
