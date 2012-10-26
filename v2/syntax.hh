#ifndef bqtEsyntaxHH
#define bqtEsyntaxHH

/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include <vector>
#include <string>
#include <array>

#include "attr.hh"
#include "char32.hh"
#include "ptr_array.hh"

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
            unsigned short tgt_state;

            unsigned char recolor = 0;
            unsigned char strings:6;      // 0=no strings, 1=strings, 2=istrings
            bool     noeat:1;
            bool     buffer:1;

            // If the strings option is taken, the stringtable is searched
            // for the contents of buffer (that started when "buffer" option
            // was hit last time), and target is chosen based on that.

            // std::map is not used for this stringtable, because
            // it does not work with case-insensitive matching.
            std::vector< std::pair<std::string, unsigned short> > stringtable;

            // The string table is sorted ddifferently depending on whether
            // strings are matched in case-sensitive or insensitive manner.
            void SortStringTable();

            std::vector< std::pair<std::string, unsigned short> >::const_iterator
                SearchStringTable(const std::string& s) const;
        };

        // An array is precalculated for 256 characters.
        /* Because we can possibly run in DOS environment as well, it makes sense
         * to reduce the memory usage of this program as much as possible (hah!).
         * To that end, we use this size-optimized "pointer_array" structure
         * rather than a simple option* options[256];.
         */
        pointer_array<unsigned short, 256> options;

        state() = default;
        state(state&&) = default;
        state& operator=(state&&) = default;
    };

    std::vector<state>         states;
    std::vector<state::option> options;

public:
    // Load the given .jsf file
    void Parse(const std::string& fn, bool quiet = false);
    void Parse(FILE* fp, bool quiet = false);

    // Guess which .jsf file is best choice for the given file,
    // and load that one.
    void LoadForFile(const std::string& fn, bool quiet = false);

    struct ApplyState
    {
        std::string    buffer;
        unsigned short state_no;
        unsigned char  recolor;
        char           c;
        bool           noeat;

        ApplyState() : buffer(), state_no(0), recolor(0), c('?'), noeat(false) { }

        bool operator== (const ApplyState& b) const
        {
            return state_no == b.state_no
                && noeat    == b.noeat
                && recolor  == b.recolor
                && c        == b.c
                && buffer   == b.buffer;
        }
        bool operator!= (const ApplyState& b) const { return !operator==(b); }
    };

    template<typename GetFunc,      /* int() */
             typename RecolorFunc>  /* void(unsigned n,AttrType attr) */
    void Apply(ApplyState& state, GetFunc&& Get, RecolorFunc&& Recolor)
    {
        if(states.empty()) return;
        if(state.state_no >= states.size()) state.state_no = 0;

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

            const auto& s = states[state.state_no];

            Recolor(state.recolor, s.attr);
            state.recolor = 0;

            const auto& o = options[s.options.Get( (unsigned char) state.c )];
            state.recolor  = o.recolor;
            state.noeat    = o.noeat;
            state.state_no = o.tgt_state; // Choose the default target state
            if(o.strings)
            {
                auto i = o.SearchStringTable(state.buffer);
                if(i != o.stringtable.end())
                {
                    state.state_no = i->second;
                    state.recolor  = state.buffer.size() + 1;
                }
                state.buffer.clear();
            }
            else if(!state.buffer.empty() && !state.noeat)
            {
                state.buffer += state.c;
            }
            if(o.buffer)
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
