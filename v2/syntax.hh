#ifndef bqtEsyntaxHH
#define bqtEsyntaxHH

/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include <vector>
#include <list>
#include <string>

#include "attr.hh"
#include "char32.hh"

/* JSF file parser and applier
 * JSF = Joe Syntax Format.
 *
 * It uses syntax files from Joseph Allen's editor.
 */
class JSF
{
    struct state
    {
        // In JSF, the states have attributes (colors), not the transitions.
        AttrType attr;

        // Single-character lookahead determines the action taken.
        struct option
        {
            // Target state when option is matched
            union
            {
                state*       stateptr;
                std::string* nameptr;
            };

            unsigned char recolor = 0;
            unsigned char strings = 0;     // 0=no strings, 1=strings, 2=istrings
            bool     noeat    = false;
            bool     buffer   = false;

            // If the strings option is taken, the stringtable is searched
            // for the contents of buffer (that started when "buffer" option
            // was hit last time), and target is chosen based on that.
            union stringopt // Target state when a string item is matched
            {
                state*       stateptr;
                std::string* nameptr;
            };

            // std::map is not used for this stringtable, because
            // it does not work with case-insensitive matching.
            std::vector< std::pair<std::string, stringopt> > stringtable;

            // The string table is sorted ddifferently epending on whether
            // strings are matched in case-sensitive or insensitive manner.
            void SortStringTable();

            std::vector< std::pair<std::string, stringopt> >::const_iterator
                SearchStringTable(const std::string& s) const;
        };

        // An array is precalculated for 256 characters.
        option* options[256];

        state() : options{{}} {}
        state(state&& b) = default;
        state& operator=(const state&) = delete;
        state(const state&) = delete;
        ~state();
    };

    std::list<state> states;

public:
    // Load the given .jsf file
    void Parse(const std::string& fn, bool quiet = false);
    void Parse(FILE* fp, bool quiet = false);

    // Guess which .jsf file is best choice for the given file,
    // and load that one.
    void LoadForFile(const std::string& fn, bool quiet = false);

    struct ApplyState
    {
        std::string   buffer;
        state*        s;
        unsigned char recolor;
        char          c;
        bool          noeat;

        ApplyState() : buffer(), s(nullptr), recolor(0), c('?'), noeat(false) { }

        bool operator== (const ApplyState& b) const
        {
            return s       == b.s
                && noeat   == b.noeat
                && recolor == b.recolor
                && c       == b.c
                && buffer  == b.buffer;
        }
        bool operator!= (const ApplyState& b) const { return !operator==(b); }
    };

    template<typename GetFunc,      /* int() */
             typename RecolorFunc>  /* void(unsigned n,AttrType attr) */
    void Apply(ApplyState& state, GetFunc&& Get, RecolorFunc&& Recolor)
    {
        if(states.empty())
        {
            state.s = 0;
            return;
        }

        if(!state.s)
        {
            state.s = &states.front();
        }

        for(;;)
        {
            /*fprintf(stdout, "[State %s]", state.s->name);*/
            if(state.noeat)
            {
                state.noeat = false;
                if(!state.recolor) state.recolor = 1;
            }
            else
            {
                int ch = Get();
                if(ch < 0) break;
                state.c = UnicodeToASCIIapproximation(ch);
                state.recolor += 1;
            }

            Recolor(state.recolor, state.s->attr);
            state.recolor = 0;

            const auto* o = state.s->options[ (unsigned char) state.c ];
            state.recolor = o->recolor;
            state.noeat   = o->noeat;
            state.s = o->stateptr; // Choose the default target state
            if(o->strings)
            {
                auto i = o->SearchStringTable(state.buffer);
                if(i != o->stringtable.end())
                {
                    state.s       = i->second.stateptr;
                    state.recolor = state.buffer.size() + 1;
                }
                state.buffer.clear();
            }
            else if(!state.buffer.empty() && !state.noeat)
            {
                state.buffer += state.c;
            }
            if(o->buffer)
            {
                state.buffer.clear();
                state.buffer += state.c;
            }
        }
    }

    // Default constructor.
    JSF() = default;

    // Delete copy-construct and assign operators,
    // so we won't have to worry about them.
    JSF& operator=(const JSF&) = delete;
    JSF(const JSF&) = delete;
    // Move-construct and move-assign are fine though.
    JSF& operator=(JSF&&) = default;
    JSF(JSF&&) = default;
};

#endif
