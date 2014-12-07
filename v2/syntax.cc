#include <cstring>
#include <algorithm>
#include <utility>
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

    std::vector< std::pair<std::string, AttrType> > colornames;
    std::vector< std::string > statenames;

    auto GetStateByName = [&](const std::string& s) -> unsigned short
    {
        auto i = std::lower_bound(statenames.begin(), statenames.end(), s);
        if(i == statenames.end() || *i != s)
        {
            if(!quiet)
                std::fprintf(stdout, "Unknown state name: '%s'\n", s.c_str());
            return 0;
        }
        return std::distance(statenames.begin(), i);
    };
    auto GetColorByName = [&](const std::string& s) -> const AttrType
    {
        auto i = std::lower_bound(colornames.begin(), colornames.end(), s,
            []( const std::pair<std::string, AttrType>& p1,
                const std::string& p2) -> bool
            {
                return p1.first < p2;
            });
        if(i == colornames.end() || i->first != s)
        {
            if(!quiet)
                std::fprintf(stdout, "Unknown color name: '%s'\n", s.c_str());
            return AttrType();
        }
        return i->second;
    };

    // Count states and options
    if(true)
    {
        std::size_t n_states = 0;
        std::size_t n_colors = 0;
        while(std::fgets(Buf, sizeof(Buf), fp))
        {
            if(Buf[0] == ':') ++n_states;
            if(Buf[0] == '=') ++n_colors;
        }
        std::rewind(fp);
        statenames.reserve(n_states);
        colornames.reserve(n_colors);
        if(n_states > 65535)
        {
            if(!quiet)
                std::fprintf(stdout, "JSF file error: Too many states! 65535 is the limit.\n");
        }
        // Save colors and state-name
        while(std::fgets(Buf, sizeof(Buf), fp))
        {
            cleanup(Buf);
            if(Buf[0] == '=') // Color
            {
                char* line = Buf+1;
                while(*line==' '||*line=='\t') ++line;
                char* namebegin = line;
                while(*line && *line != ' ' && *line!='\t') ++line;
                char* nameend = line;
                while(*line==' '||*line=='\t') ++line;
                *nameend = '\0';
                colornames.emplace_back( namebegin, AttrType::ParseJSFattribute(line) );
            }
            if(Buf[0] == ':') // State
            {
                char* line = Buf+1;
                while(*line==' '||*line=='\t') ++line;
                char* namebegin = line;
                while(*line && *line != ' ' && *line!='\t') ++line;
                char* nameend = line;
                while(*line==' '||*line=='\t') ++line;
                *nameend = '\0';
                // Now namebegin=name, line=colorname
                statenames.push_back(namebegin);
                // Can't assign data into states[] yet, because
                // the relative ordering of statenames will change.
            }
        }
        std::sort(colornames.begin(), colornames.end(),
                    []( const std::pair<std::string,AttrType>& a,
                        const std::pair<std::string,AttrType>& b) -> bool
                    {
                        return a.first < b.first;
                    });
        std::sort(statenames.begin(), statenames.end());
        std::rewind(fp);
        states.resize(n_states);
    }

    unsigned short current_state = 0;

    // Scan for states
    while(std::fgets(Buf, sizeof(Buf), fp))
    {
        // Remove comments and trailing space from the buffer
        cleanup(Buf);

        // Identify the contents of the buffer
        switch(Buf[0])
        {
            case '=': // Parse color declaration
                break; // Already done

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
                current_state              = GetStateByName(namebegin);
                states[current_state].attr = GetColorByName(line);
                break;
            }
            case ' ':
            case '\t': // Parse actual flesh of the state (pattern and definition)
            {
                char* line = Buf;
                while(*line == ' ' || *line == '\t') ++line;

                state::option* o = new state::option;
                o->strings = 0;
                o->noeat   = false;
                o->buffer  = false;
                unsigned uses = 0;

                {auto& s = states[current_state];
                switch(*line)
                {
                    case '*': // Match every character
                        for(unsigned a=0; a<256; ++a)
                            s.options.Set(a, o);
                        uses += 256;
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
                                do { s.options.Set(first, o); ++uses; }
                                while(first++ != (unsigned char)*line);
                            }
                            else
                            {
                                s.options.Set(first, o);
                                ++uses;
                            }
                        }
                        if(*line == '"') ++line;
                        break;
                }
                if(!uses && !quiet)
                    fprintf(stderr, "Warning: JSF option never used\n");
                }//end scope for states[current_state]
                while(*line == ' ' || *line == '\t') ++line;
                char* namebegin = line;
                while(*line && *line != ' ' && *line!='\t') ++line;
                char* nameend   = line;
                while(*line == ' ' || *line == '\t') ++line;
                *nameend = '\0';

                o->tgt_state = GetStateByName(namebegin);

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
                    // First, count the number of strings
                    std::size_t n_strings = 0;
                    auto p = std::ftell(fp);
                    while(std::fgets(Buf, sizeof(Buf), fp))
                    {
                        cleanup(Buf);
                        line = Buf;
                        while(*line == ' ' || *line == '\t') ++line;
                        if(std::strcmp(line, "done") == 0) break;
                        ++n_strings;
                    }
                    std::fseek(fp, p, SEEK_SET);
                    o->stringtable.reserve(n_strings);
                    // Then read the strings
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
                        /*while(*line != '\0') ++line;
                        unsigned char* value_end   = (unsigned char*) line;
                        *value_end++ = '\0';*/

                        o->stringtable.push_back( {key_begin, GetStateByName(value_begin)} );
                    }
                    o->SortStringTable();
                }
            }
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
            [](const std::pair<std::string, unsigned short>& a,
               const std::pair<std::string, unsigned short>& b)
            {
                return a.first < b.first;
            } );
    else
        std::sort(stringtable.begin(), stringtable.end(),
            [](const std::pair<std::string, unsigned short>& a,
               const std::pair<std::string, unsigned short>& b)
            {
                return strcasecmp(a.first.c_str(), b.first.c_str()) < 0;
            } );
}


std::vector< std::pair<std::string, unsigned short> >::const_iterator
    JSF::state::option::SearchStringTable(const std::string& s) const
{
    auto i = (strings == 1)
        ? // Case-sensitive matching
          std::lower_bound(stringtable.begin(), stringtable.end(), s,
            [](const std::pair<std::string, unsigned short>& a,
               const std::string& s)
            {
                return a.first < s;
            } )
        :  // Case-insensitive
          std::lower_bound(stringtable.begin(), stringtable.end(), s,
            [](const std::pair<std::string, unsigned short>& a,
               const std::string& s)
            {
                return strcasecmp(a.first.c_str(), s.c_str()) < 0;
            } );
    if(i == stringtable.end() || i->first != s)
        return stringtable.end();
    return i;
}
