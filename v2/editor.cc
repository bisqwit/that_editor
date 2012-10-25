#include <cstdio>
#include <cstdlib>

#include <ctime>

#include "editor.hh"
#include "char32.hh"
#include "cpuinfo.hh"

void Editor::UndoQueueType::AddUndo(const UndoEvent& event)
{
    if(UndoAppendOk && !UndoQueue.empty())
    {
        UndoEvent& prev = UndoQueue.back();
        /*if(event.n_delete == 0 && prev.n_delete == 0
        && event.x == prev.x && event.y == prev.y
          )
        {
            prev.insert_chars.insert(
                prev.insert_chars.end(),
                event.insert_chars.begin(),
                event.insert_chars.end());
            return;
        }*/
        if(event.insert_chars.empty() && prev.insert_chars.empty())
        {
            prev.n_delete += event.n_delete;
            return;
        }
    }

    UndoQueue.push_back(event);
}

void Editor::UndoQueueType::AddRedo(const UndoEvent& event)
{
    RedoQueue.push_back(event);
}

void Editor::PerformEdit(
    std::size_t x, std::size_t y,
    std::size_t n_delete,
    const std::vector<Cell>& insert_chars,
    unsigned char DoingUndo)
{
    std::size_t eol_x = EditLines[y].size();
    if(eol_x > 0 && EditLines[y].back().GetCh() == InternalNewline)
        --eol_x;

    if(x > eol_x) x = eol_x;

    UndoEvent event = {x,y, n_delete, {}};

    chars_file += insert_chars.size();

    if(DoingUndo)
    {
        std::basic_string<char32_t> buf;
        for(std::size_t b=insert_chars.size(), a=0; a<b && a<200; ++a)
        {
            char32_t c = insert_chars[a].GetCh();
            if(c == '\n')
                { buf += '\\'; buf += 'n'; }
            else if(c == '\r')
                { buf += '\\'; buf += 'r'; }
            else if(c == '\t')
                { buf += '\\'; buf += 't'; }
            else
                buf += c;
        }
        StatusPrintf("Edit%u @%u,%u: Delete %u, insert '%s'",
            UndoQueue.UndoAppendOk, x, y, n_delete, buf);
    }

    // Is there something to delete?
    if(n_delete > 0)
    {
        std::size_t n_lines_deleted = 0;
        // If the deletion spans across newlines, concatenate those lines first
        while(n_delete >= EditLines[y].size() - x
           && y+1+n_lines_deleted < EditLines.size())
        {
            ++n_lines_deleted;

            ForAllCursors([&](Anchor& c) {
                if(c.y == y+n_lines_deleted)
                {
                    c.y = y;
                    c.x += EditLines[y].size();
                } });

            EditLines[y].insert( EditLines[y].end(),
                EditLines[y+n_lines_deleted].begin(),
                EditLines[y+n_lines_deleted].end() );
            EditLines[y].MarkChanged(); // Unnecessary because of what is to come,
            // but let's keep that line for clarity and failsafe.
        }
        if(n_lines_deleted > 0)
        {
            ForAllCursors([&](Anchor& c) {
                if(c.y > y)
                    c.y -= n_lines_deleted; });

            EditLines.erase(EditLines.begin()+y+1,
                            EditLines.begin()+y+1+n_lines_deleted);
            // Mark the line immediately following the deletion as dirty,
            // syntax-wise, because the context of the line has changed.
            if(y+1 < EditLines.size())
                EditLines[y+1].dirtiness |= EditLine::dirty_syntax;
        }
        // Now the deletion can begin
        if(n_delete > EditLines[y].size()-x)
            n_delete = EditLines[y].size()-x;

        chars_file += event.insert_chars.size();
        event.insert_chars.insert(
            event.insert_chars.end(),
            EditLines[y].begin() + x,
            EditLines[y].begin() + x + n_delete);
        chars_file -= event.insert_chars.size();

        EditLines[y].erase(
            EditLines[y].begin() + x,
            EditLines[y].begin() + x + n_delete);

        // This line is dirty, because it has been changed.
        EditLines[y].MarkChanged();

        ForAllCursors([&](Anchor& c) {
            if(c.y == y && c.x > x+n_delete)
                c.x -= n_delete;
            else if(c.y == y && c.x > x)
                c.x = x; });
    }

    // Next, check if there is something to insert
    if(!insert_chars.empty())
    {
        std::size_t insert_length = insert_chars.size();
        event.n_delete = insert_length;

        std::size_t insert_newline_count = 0;
        for(std::size_t p=0; p<insert_length; ++p)
            if( insert_chars[p].GetCh() == InternalNewline )
                ++insert_newline_count;

        if(insert_newline_count > 0)
        {
            std::vector<EditLine> new_lines( insert_newline_count, EditLine(1,InternalNewline) );
            // ^ All of these lines are automatically marked dirty by their constructor.

            // Move the trailing part from current line to the beginning of last "new" line
            new_lines.back().assign( EditLines[y].begin() + x, EditLines[y].end() );

            // Remove the trailing part from that line
            EditLines[y].erase(  EditLines[y].begin() + x, EditLines[y].end() );

            // But keep the newline character
            EditLines[y].push_back( Cell{InternalNewline} );

            // Mark that line changed
            EditLines[y].MarkChanged();

            // Insert these new lines (all dirty, syntaxcoloringwise)
            EditLines.insert(
                EditLines.begin()+y+1,
                new_lines.begin(),
                new_lines.end() );

            // Update cursors
            ForAllCursors([&](Anchor& c) {
                if(c.y == y && c.x >= x)
                {
                    c.y += insert_newline_count;
                    c.x -= x;
                }
                else if(c.y > y)
                {
                    c.y += insert_newline_count;
                } });
        }

        std::size_t insert_beginpos = 0;
        while(insert_beginpos < insert_length)
        {
            if( insert_chars[insert_beginpos].GetCh() == InternalNewline)
                { x = 0; ++y; ++insert_beginpos; }
            else
            {
                unsigned p = insert_beginpos;
                while(p < insert_length && insert_chars[p].GetCh() != InternalNewline)
                    ++p;

                unsigned n_inserted = p - insert_beginpos;
                EditLines[y].insert(
                    EditLines[y].begin() + x,
                    insert_chars.begin() + insert_beginpos,
                    insert_chars.begin() + p );

                // Mark that line changed
                EditLines[y].MarkChanged();

                ForAllCursors([&](Anchor& c) {
                    if(&c != &BlockBegin
                    && &c != &BlockEnd)
                        if(c.y == y && c.x >= x)
                            c.x += n_inserted;
                }, false); // All cursors except blockbegin & blockend

                x += n_inserted;
                insert_beginpos = p;
            }
        }
    }
    switch(DoingUndo)
    {
        case 0: // normal edit
            UndoQueue.ClearRedo();
            UndoQueue.AddUndo(event);
            break;
        case 1: // undo
            UndoQueue.AddRedo(event);
            break;
        case 2: // redo
            UndoQueue.AddUndo(event); // add undo, but don't reset redo
            break;
    }

    UnsavedChanges = true;
}

// Return an array containing all cells from the selected area,
// including newlines where relevant.
std::vector<Editor::Cell> Editor::GetSelectedBlock() const
{
    std::vector<Cell> block;

    for(std::size_t y = BlockBegin.y; y <= BlockEnd.y; ++y)
    {
        std::size_t x0 = 0, x1 = EditLines[y].size();

        if(y == BlockBegin.y) x0 = BlockBegin.x;
        if(y == BlockEnd.y)   x1 = BlockEnd.x;

        block.insert(block.end(),
            EditLines[y].begin() + x0,
            EditLines[y].begin() + x1);
    }

    return block;
}

// Put the cursor to the corresponding parenthesis
void Editor::Act_FindPair(std::size_t which_window)
{
    auto& Cur = windows[which_window].Cur;

    int      PairDir;
    char32_t PairChar;
    AttrType PairColor = EditLines[Cur.y][Cur.x].GetAttr();
    switch(EditLines[Cur.y][Cur.x].GetCh())
    {
        case U'{': PairChar = U'}'; PairDir = 1; break;
        case U'[': PairChar = U']'; PairDir = 1; break;
        case U'(': PairChar = U')'; PairDir = 1; break;
        case U'}': PairChar = U'{'; PairDir = -1; break;
        case U']': PairChar = U'['; PairDir = -1; break;
        case U')': PairChar = U'('; PairDir = -1; break;
        default: return;
    }
    int balance = 0;
    auto testx = Cur.x, testy = Cur.y;

    if(PairDir > 0)
        for(;;)
        {
            if(++testx >= EditLines[testy].size())
                { testx=0; ++testy; if(testy >= EditLines.size()) return; }
            if(EditLines[testy][testx].GetAttr() != PairColor) continue;
            char32_t c = EditLines[testy][testx].GetCh();
            if(balance == 0 && c == PairChar) { Cur.x = testx; Cur.y = testy; return; }
            if(c == U'{' || c == U'[' || c == U'(') ++balance;
            if(c == U'}' || c == U']' || c == U')') --balance;
        }
    else
        for(;;)
        {
            if(testx == 0)
                { if(testy == 0) return;
                  testx = EditLines[--testy].size() - 1; }
            else
                --testx;
            if(EditLines[testy][testx].GetAttr() != PairColor) continue;
            char32_t c = EditLines[testy][testx].GetCh();
            if(balance == 0 && c == PairChar) { Cur.x = testx; Cur.y = testy; return; }
            if(c == U'{' || c == U'[' || c == U'(') ++balance;
            if(c == U'}' || c == U']' || c == U')') --balance;
        }
}

void Editor::Act_BlockIndent(int offset)
{
    std::size_t firsty = BlockBegin.y, lasty = BlockEnd.y;
    if(BlockEnd.x == 0) lasty -= 1;

    std::size_t min_indent = ~std::size_t(0), max_indent = 0;
    for(std::size_t y=firsty; y<=lasty; ++y)
    {
        std::size_t indent = 0;
        while(indent < EditLines[y].size() && EditLines[y][indent].GetCh() == U' ')
            ++indent;
        if(EditLines[y][indent].GetCh() == InternalNewline) continue;
        if(indent < min_indent) min_indent = indent;
        if(indent > max_indent) max_indent = indent;
    }
    if(offset > 0)
    {
        std::vector<Cell> indentbuf{ (unsigned)offset, Cell{' '}};
        for(std::size_t y = firsty; y <= lasty; ++y)
        {
            std::size_t indent = 0;
            while(indent < EditLines[y].size() && EditLines[y][indent].GetCh() == U' ')
                ++indent;
            if(EditLines[y][indent].GetCh() == InternalNewline) continue;
            PerformEdit(0u,y, 0u, indentbuf);
        }
        if(BlockBegin.x > 0) BlockBegin.x += offset;
        if(BlockEnd.x   > 0) BlockEnd.x   += offset;
    }
    else if(min_indent >= (unsigned)-offset)
    {
        unsigned outdent = -offset;
        std::vector<Cell> empty;

        for(std::size_t y=firsty; y<=lasty; ++y)
        {
            std::size_t indent = 0;
            while(indent < EditLines[y].size() && EditLines[y][indent].GetCh() == U' ')
                ++indent;
            if(EditLines[y][indent].GetCh() == InternalNewline) continue;
            if(indent < outdent) continue;
            PerformEdit(0u,y, outdent, empty);
        }
        if(BlockBegin.x >= outdent) BlockBegin.x -= outdent;
        if(BlockEnd.x   >= outdent) BlockEnd.x   -= outdent;
    }
}

void Editor::FileNew()
{
    // Initialize file with three emptylines
    EditLines.assign( 3, EditLine(1, InternalNewline) );
    CurrentFilename.clear();
    CurrentFilenameWithoutPath = "[untitled]";

    // Place all cursors in the beginning of the file
    ForAllCursors( [](Anchor& c) -> void { c = Anchor(); } );
    UnsavedChanges = false;
    UndoQueue.ClearUndo();
    UndoQueue.ClearRedo();

    // Count the number of characters in the file
    FileCountCharacters();
}

bool Editor::FileLoad(const char* fn)
{
    std::fprintf(stderr, "Loading '%s'...\n", fn);

    std::FILE* fp = std::fopen(fn, "rb");
    if(!fp)
    {
        std::perror(fn);
        return false;
    }

    EditLines.clear();
    EditLine CurrentLine;

    /* We have several different ways that newlines can be handled:
     *
     * ReadCR:     CR          .
     * ReadLF:     LF          .
     * ReadCRLF:   CR LF       .
     * ReadBoth:   CR LF? | LF .
     */
    enum { CR = 13, LF = 10, TAB = 9 };
    auto MakeNewline = [&]()
    {
        // InternalNewline must always be the last Cell on the line.
        CurrentLine.push_back( InternalNewline );
        EditLines.push_back(CurrentLine);
        CurrentLine.clear();
    };
    auto AppendCharacter = [&](char32_t c)
    {
        if(c == TAB && !InputPreserveTabs)
        {
            std::size_t nextstop = CurrentLine.size() + FileTabSize;
            nextstop -= nextstop % FileTabSize;
            CurrentLine.resize( nextstop, Cell{U' '} );
        }
        else
            CurrentLine.push_back( Cell{c} );
    };

    int c;
    // TODO: byte->char32_t conversion
    while((c = std::fgetc(fp)) >= 0)
    {
        switch(InputLineStyle)
        {
            case InputLineStyles::ReadCR:
                if(c == CR) { MakeNewline(); continue; }
                break;
            case InputLineStyles::ReadLF:
                if(c == LF) { MakeNewline(); continue; }
                break;
            case InputLineStyles::ReadCRLF: redo1:
                if(c == CR)
                {
                    int c2 = std::fgetc(fp);
                    if(c2 == LF) { MakeNewline(); continue; }
                    // Not CRLF. Append the CR verbatim, and deal with second character.
                    AppendCharacter(c);
                    c = c2;
                    goto redo1;
                }
                break;
            case InputLineStyles::ReadBoth: redo2:
                if(c == LF) { MakeNewline(); continue; }
                if(c == CR)
                {
                    MakeNewline();
                    int c2 = std::fgetc(fp);
                    if(c2 == LF) continue;
                    c = c2;
                    goto redo2;
                }
        }
        AppendCharacter(c);
    }
    // This editor forces the last character of the file be a newline.
    if(!CurrentLine.empty()) MakeNewline();

    // Keep the filename
    CurrentFilename = fn;
    // Keep also a version without the pathname, for the status lines
    auto p = CurrentFilename.rfind('/' ); p = (p != std::string::npos) ? p+1 : 0;
    auto q = CurrentFilename.rfind('\\'); q = (q != std::string::npos) ? q+1 : 0;
    CurrentFilenameWithoutPath = CurrentFilename.substr(std::max(p,q));

    std::fclose(fp);

    // Place all cursors in the beginning of the file
    ForAllCursors( [](Anchor& c) { c = Anchor(); } );
    UnsavedChanges = false;
    UndoQueue.ClearUndo();
    UndoQueue.ClearRedo();

    // Count the number of characters in the file
    FileCountCharacters();

    SyntaxColorer.LoadForFile(CurrentFilename);

    return true;
}

bool Editor::FileSave(const char* fn)
{
    std::FILE* fp = std::fopen(fn, "wb");
    if(!fp)
    {
        std::perror(fn);
        return false;
    }
    enum { CR = 13, LF = 10, TAB = 9 };
    for(auto& l: EditLines)
    {
        std::size_t length = l.size();
        if(length > 0 && l.back().GetCh() == InternalNewline)
            --length; // Ignore the newline character

        std::size_t a=0;
        if(OutputCreateTabs)
        {
            // Count the number of spaces in the beginning
            while(a < length && l[a].GetCh() == U' ') ++a;
            std::size_t num_tabs = a / FileTabSize;
            for(a = 0; a < num_tabs; ++a)
                std::fputc( TAB, fp );
            a *= FileTabSize;
        }
        for(; a<length; ++a)
        {
            char32_t c = l[a].GetCh();
            fputc( c, fp ); //  TODO: char32_t->byte conversion
        }

        switch(OutputLineStyle)
        {
            case OutputLineStyles::WriteCR:
                std::fputc( CR, fp);
                break;
            case OutputLineStyles::WriteCRLF:
                std::fputc( CR, fp);
                // passthru
            case OutputLineStyles::WriteLF:
                std::fputc( LF, fp);
        }
    }
    std::fclose(fp);

    StatusPrintf("Saved %lu bytes to %s", EditLines.size(), fn);
    Act_Refresh();

    UnsavedChanges = false;
    return true;
}

void Editor::FileCountCharacters()
{
    chars_file = 0;
    for(const auto& e: EditLines) chars_file += e.size();
}

void Editor::Window::AdjustCursor(bool center)
{
    // There are two possible ways to catch up with the cursor.
    // A: Place the window such that the cursor is in the middle of the window
    // B: Scroll the window minimally towards the cursor until the cursor is visible.
    // 
    // For horizontal scrolling, we always do B.
    // For vertical scrolling, the parameter chooses between A(true) or B(false).

    // Scroll the window horizontally in increments of 8 characters
    if(Cur.x < Win.x || Cur.x >= Win.x + Dim.x)
    {
        // If the window width is 8 characters or less,
        // we scroll in increments of 1 character.
        if(Dim.x > 8)
            Win.x = Cur.x - Cur.x % 8;
        else
            Win.x = Cur.x;
    }

    // Is the cursor is vertically out of range?
    if(Cur.y < Win.y || Cur.y >= Win.y + Dim.y)
    {
        if(center)
        {
            Win.y = Cur.y - Dim.y / 2;
        }
        else
        {
            if(Cur.y < Win.y) Win.y = Cur.y;
            else              Win.y = Cur.y - Dim.y + 1;
        }
    }
}

// Are there possibly incorrectly colored lines remaining?
std::size_t Editor::SyntaxColorIncomplete() const
{
    for(std::size_t b=EditLines.size(), a=0; a<b; ++a)
        if(EditLines[a].dirtiness & EditLine::dirty_syntax)
            return a;
    return ~std::size_t(0);
}

// Update the coloring for the next line
void Editor::UpdateSyntaxColor(std::size_t which_window)
{
    // Which line is next?
    std::size_t which_line = SyntaxColorIncomplete();
    if(which_line >= EditLines.size()) return;

    // However, before actually going forward with which_line,
    // check if the this line is above our viewport and if
    // any lines in the current viewport look HORRIBLE.
    //
    // If that is the case, jump forward to fix the horrible
    // lines first, even if a bit inaccurately, and then go
    // back to whatever was it that really needed fixing.
    // However, don't mark as clean those lines that we
    // quickly fixed, because quick&dirty is still dirty.
    //
    // Apply the quick&dirty fixing also to the line
    // that the cursor is at.

    if(which_window < windows.size())
    {
        const auto& w = windows[which_window];

        if(which_line < w.Win.y)
        {
            for(std::size_t l=0; l<w.Dim.y; ++l)
            {
                std::size_t lineno = w.Win.y + l;
                if(lineno >= EditLines.size()) break;

                auto& line = EditLines[lineno];

                // Check if the line is dirty, looks ugly, and hasn't been quickfixed yet.
                if((line.dirtiness & EditLine::dirty_syntax)
                && !(line.dirtiness & EditLine::dirty_quick)
                && (line[0].GetAttr()                == UnknownColor
                ||  line[ line.size()*3/4].GetAttr() == UnknownColor) )
                {
                    // Fix the line, the quick & dirty way.
                    UpdateSyntaxColorThatLine(lineno);
                    // It still remains dirty.
                    line.dirtiness |= EditLine::dirty_syntax | EditLine::dirty_quick;
                    // Only deal with one line at a time.
                    return;
                }
            }
        }

        // "fix" the line that the cursor is at.
        if(w.Cur.y < EditLines.size())
        {
            auto& line = EditLines[w.Cur.y];
            // If it doesn't need fixing, don't try to fix it.
            // Also don't quick-fix it twice.
            if((line.dirtiness & EditLine::dirty_syntax)
            && !(line.dirtiness & EditLine::dirty_quick))
            {
                UpdateSyntaxColorThatLine(w.Cur.y);
                // It still remains dirty, unless if was actually
                // the line that we were going to fix in the first place.
                if(which_line != w.Cur.y)
                {
                    // To avoid fixing the same line over and over again,
                    // we set and check the dirty_quick flag.
                    // The flag is cleared when the line is _properly_ rendered.
                    line.dirtiness |= EditLine::dirty_syntax | EditLine::dirty_quick;
                }
                // Only deal with one line at a time.
                return;
            }
        }
    }

    UpdateSyntaxColorThatLine(which_line);
}

void Editor::UpdateSyntaxColorThatLine(std::size_t which_line)
{
    // Continue from the state of the previous line
    // (If it is the first line, start from default state)
    JSF::ApplyState syntax_state;
    if(which_line > 0)
        syntax_state = EditLines[which_line-1].syntax_state;

    auto& line = EditLines[which_line];

    std::size_t x = 0;
    unsigned pending_recolor {};
    AttrType pending_attr    {};

    auto FlushColor = [&]() ->void
    {
        unsigned n = pending_recolor;
        //fprintf(stdout, "Recolors %u as %02X\n", n, attr);
        std::size_t px = x, py = which_line;
        for(; n > 0; --n)
        {
            if(px-- == 0)
            {
                // We are recoloring previous lines?
                // We can do that... Technically, but the dirty-flag
                // mechanism will be quite broken when this happens.
                if(!py--) break;
                px = EditLines[py].size()-1;
            }
            if(px < EditLines[py].size())
                EditLines[py][px].SetAttr(pending_attr);
        }
        pending_recolor = 0;
    };

    SyntaxColorer.Apply(syntax_state,
        // JSF interface function: Get next character. Negative = not available right now
        [&]()->int
        {
            FlushColor();
            if(x >= line.size()) return -1;

            // Note: This also passes InternalNewline to the JSF engine.
            //       This is intentional and really, required.
            //       Newlines are an important part of the syntax.
            char32_t c = line[x++].GetCh();
            if(c == InternalNewline) return '\n';
            return c;
        },
        // JSF interface function: Recolor n last characters with attr
        [&](unsigned n, AttrType attr) -> void
        {
            if(n < pending_recolor) FlushColor();
            pending_recolor = n;
            pending_attr    = attr;
        });

    // This line is no longer dirty syntaxwise,
    // but its appearance may have changed.
    line.dirtiness =
        (line.dirtiness & ~(EditLine::dirty_syntax | EditLine::dirty_quick))
      | EditLine::dirty_cell;

    // Did this change the outcome of the line in any way?
    if(syntax_state != line.syntax_state)
    {
        // It did! Save the changed state.
        line.syntax_state = std::move(syntax_state);
        // Also mark the next line suspect for syntax checking,
        // because syntax changes (such as comments) sometimes
        // cascade through the file.
        if(which_line+1 < EditLines.size())
            EditLines[which_line+1].dirtiness |= EditLine::dirty_syntax;
    }
}

// For StatusRender functions
static const char slide[] = {3,7,7,7,7,3,3,2};
static const char slide2[] = {15,15,15,14,7,6,8,0,0};
#define f(v) v / 16.0f
static const float Bayer[16] = { f(0),f(8),f(4),f(12),f(2),f(10),f(6),f(14),
                                 f(3),f(11),f(7),f(15),f(1),f(9),f(5),f(13) };
#undef f

void Editor::StatusRenderTop(std::vector<Cell>& top, std::size_t which_window)
{
    const std::size_t Width = top.size();
    auto& w = windows[which_window];
    bool ShowFilename = Width > 60;
    bool ShowMHz      = Width >= 65;

    auto t = std::time(nullptr);
    const auto* tm = std::localtime(&t);
    // ^Note: would use std::chrono, but not supported well
    //        by DJGPP yet as of GCC 4.7.2

    auto Buf1 = Printf(U"%s%sRow %-5u/%u Col %-5u",
        ShowFilename?CurrentFilenameWithoutPath:"",
        ShowFilename?" ":"",
        w.Cur.y+1,
        EditLines.size(),
        w.Cur.x+1);
    auto Buf2 = Printf(U"%02d:%02d:%02d %u/%u",
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec,
        chars_file, chars_typed);
    if(ShowMHz)
    {
        Buf2 += Printf(U" %d MHz", CPUinfo()*1e-6);
    }

    // X coordinate ranges for buf1 and buf2
    std::size_t x1a = Width*12/70; if(ShowFilename) x1a = 7;
    std::size_t x2a = Width*55/70;
    std::size_t x1b = x1a + Buf1.size();
    std::size_t x2b = x2a + Buf2.size();

    for(unsigned x=0; x<Width; ++x)
    {
        float xscale = x * (sizeof(slide)-1) / float(Width-1);
        char c1 = slide[unsigned( xscale + Bayer[0*8 + (x&7)] )]; //background
        char c2 = slide[unsigned( xscale + Bayer[1*8 + (x&7)] )]; //foreground
        char32_t ch = 0xDC;
        //if(C64palette) { c1 = 7; c2 = 0; ch = U' '; }

        /**/ if(x == 0 && WaitingCtrl)                      { c2 = 0; ch = U'^'; }
        else if(x == 1 && WaitingCtrl)                      { c2 = 0; ch = WaitingCtrl; }
        else if(x == 3 && w.InsertMode)                     { c2 = 0; ch = U'I'; }
        else if(x == 4 && UnsavedChanges)                   { c2 = 0; ch = U'*'; }
        else if(x >= x1a && x < x1b && Buf1[x-x1a] != U' ') { c2 = 0; ch = Buf1[x-x1a]; }
        else if(x >= x2a && x < x2b && Buf2[x-x2a] != U' ') { c2 = 0; ch = Buf2[x-x2a]; }

        top[x] = Cell(ch, AttrType{c2,c1});
    }
}

void Editor::StatusRenderBottom(std::vector<Cell>& bottom)
{
    const std::size_t Width = bottom.size();
    for(unsigned x=0; x<Width; ++x)
    {
        float xscale = x * (sizeof(slide)-1) / float(Width-1);
        char c1 = slide2[unsigned( xscale + Bayer[0*8 + (x&7)] )]; //background
        char c2 = slide2[unsigned( xscale + Bayer[1*8 + (x&7)] )]; //foreground
        char32_t ch = 0xDC;
        //if(C64palette) { c1 = 7; c2 = 0; ch = U' '; }

        char32_t c = x < StatusLine.size() ? StatusLine[x] : U' ';

        if(c != U' ' || c1 == c2)
        {
            c2 = 0;
            ch = c;
            if(c1 == c2) c2 = 7;
        }
        if(/*!C64palette &&*/ c != U' ')
            switch(c1)
            {
                case 8: c2 |= 7; break;
                case 0: c2 |= 8; break;
            }

        bottom[x] = Cell(ch, AttrType{c2,c1});
    }
}

void Editor::Act_PageDn(std::size_t which_window)
{
    auto& w = windows[which_window];
    auto offset = w.Cur.y - w.Win.y;
    w.Cur.y += w.Dim.y;
    if(w.Cur.y >= EditLines.size()) w.Cur.y = EditLines.size()-1;
    w.Win.y = (w.Cur.y > offset) ? w.Cur.y-offset : 0;
}

void Editor::Act_PageUp(std::size_t which_window)
{
    auto& w = windows[which_window];
    auto offset = w.Cur.y - w.Win.y;
    if(w.Cur.y > w.Dim.y) w.Cur.y -= w.Dim.y; else w.Cur.y = 0;
    w.Win.y = (w.Cur.y > offset) ? w.Cur.y - offset : 0;
}


void Editor::Act_TryUndo()
{
    if(!UndoQueue.UndoQueue.empty())
    {
        UndoEvent event = std::move(UndoQueue.UndoQueue.back());
        UndoQueue.UndoQueue.pop_back();
        PerformEdit(event.x, event.y, event.n_delete, event.insert_chars, 1);
    }
}

void Editor::Act_TryRedo()
{
    if(!UndoQueue.RedoQueue.empty())
    {
        UndoEvent event = std::move(UndoQueue.RedoQueue.back());
        UndoQueue.RedoQueue.pop_back();
        PerformEdit(event.x, event.y, event.n_delete, event.insert_chars, 2);
    }
}

void Editor::Act_WindowUp(std::size_t which_window)
{
    auto& w = windows[which_window];
    if(w.Win.y > 0) --w.Win.y;
    StatusLine.clear();
}

void Editor::Act_WindowDn(std::size_t which_window)
{
    auto& w = windows[which_window];
    if(w.Win.y+1/*+w.Dim.y*/ < EditLines.size()) ++w.Win.y;
    if(w.Cur.y < w.Win.y) w.Cur.y = w.Win.y;
    StatusLine.clear();
}

void Editor::Act_BlockMark(std::size_t which_window)
{
    auto& w = windows[which_window];
    BlockBegin = w.Cur;
    Act_Refresh();
}

void Editor::Act_BlockMarkEnd(std::size_t which_window)
{
    auto& w = windows[which_window];
    BlockEnd = w.Cur;
    Act_Refresh();
}

void Editor::Act_BlockMove(std::size_t which_window)
{
    auto& w = windows[which_window];
    std::vector<Cell> block = GetSelectedBlock(), empty;
    PerformEdit(BlockBegin.x, BlockBegin.y, block.size(), empty);
    // Note: ^ Assumes Curx,Cury get updated here.
    BlockEnd = w.Cur;
    Anchor b = w.Cur;
    PerformEdit(b.x, b.y, w.InsertMode?0u:block.size(), block);
    BlockBegin = b;
}

void Editor::Act_BlockCopy(std::size_t which_window)
{
    auto& w = windows[which_window];
    auto block = GetSelectedBlock();
    Anchor b = w.Cur;
    PerformEdit(w.Cur.x, w.Cur.y, w.InsertMode?0u:block.size(), block);
    BlockBegin = b;
    BlockEnd = w.Cur;
}

void Editor::Act_BlockDelete()
{
    std::vector<Cell> block = GetSelectedBlock(), empty;
    PerformEdit(BlockBegin.x, BlockBegin.y, block.size(), empty);
    BlockEnd = BlockBegin;
}

void Editor::Act_BlockIndent()
{
    Act_BlockIndent(+(int)IndentTabSize);
}

void Editor::Act_BlockUnindent()
{
    Act_BlockIndent(+(int)IndentTabSize);
}

void Editor::Act_CharacterInfo(std::size_t which_window)
{
    auto& w = windows[which_window];
    auto charcode = EditLines[w.Cur.y][w.Cur.x].GetCh();
    StatusPrintf(
        w.Dim.x >= 60
        ? U"Character '%c': hex=%02X, decimal=%d, octal=%03o"
        : U"Character '%c': 0x%02X = %d = '\\0%03o'",
        charcode, charcode, charcode, charcode);
}

void Editor::Act_InsertLiteralCharacter(std::size_t which_window, char32_t ch)
{
    auto& w = windows[which_window];
    std::vector<Cell> txtbuf(1, Cell{ch, BlankColor});
    PerformEdit(w.Cur.x, w.Cur.y, w.InsertMode?0u:1u, txtbuf);
}

void Editor::Act_Up(std::size_t which_window)
{
    auto& w = windows[which_window];
    if(w.Cur.y > 0) --w.Cur.y;
    if(w.Cur.y < w.Win.y) w.Win.y = w.Cur.y;
}

void Editor::Act_Dn(std::size_t which_window)
{
    auto& w = windows[which_window];
    if(w.Cur.y + 1 < EditLines.size()) ++w.Cur.y;
    if(w.Cur.y >= w.Win.y + w.Dim.y) w.Win.y = w.Cur.y - w.Dim.y + 1;
}

void Editor::Act_Home(std::size_t which_window)
{
    auto& w = windows[which_window];
    // Find the end of indenting on the line:
    std::size_t nspaces = 0;
    while(nspaces < EditLines[w.Cur.y].size()
       && EditLines[w.Cur.y][nspaces].GetCh() == U' ') ++nspaces;
    // Set the cursor at that point, or if already there, in the beginning
    w.Cur.x = (w.Cur.x != nspaces ? nspaces : 0);
    w.Win.x = 0;
}

void Editor::Act_End(std::size_t which_window)
{
    auto& w = windows[which_window];
    // Find the end of indenting on the line:
    std::size_t eol_x = EditLines[w.Cur.y].size();
    if(eol_x > 0 && EditLines[w.Cur.y][eol_x-1].GetCh() == InternalNewline) --eol_x;
    w.Cur.x = eol_x;
    w.Win.x = 0;
}

void Editor::Act_Left(std::size_t which_window)
{
    auto& w = windows[which_window];
    if(w.Cur.x >= EditLines[w.Cur.y].size())
        w.Cur.x = EditLines[w.Cur.y].size()-1;
    else if(w.Cur.x > 0) { --w.Cur.x; }
    else if(w.Cur.y > 0) { --w.Cur.y; Act_End(which_window); }
}

void Editor::Act_Right(std::size_t which_window)
{
    auto& w = windows[which_window];
    if(w.Cur.x+1 < EditLines[w.Cur.y].size())
        ++w.Cur.x;
    else if(w.Cur.y+1 < EditLines.size())
    {
        // Pressing 'right' at the end of the line goes to
        // column 0 of the next line (NOT the same as 'home').
        w.Cur.x = 0;
        ++w.Cur.y;
    }
}

void Editor::Act_WordLeft(std::size_t which_window)
{
    auto& w = windows[which_window];
    Act_Left(which_window);
    do Act_Left(which_window);
    while( (w.Cur.x > 0 || w.Cur.y > 0)
          && isalnum(EditLines[w.Cur.y][w.Cur.x].GetCh())
           );
    Act_Right(which_window);
}

void Editor::Act_WordRight(std::size_t which_window)
{
    auto& w = windows[which_window];
    while( w.Cur.y < EditLines.size()
        && w.Cur.x < EditLines[w.Cur.y].size()
        && isalnum(EditLines[w.Cur.y][w.Cur.x].GetCh()) )
        Act_Right(which_window);
    do Act_Right(which_window);
    while( w.Cur.y < EditLines.size()
        && w.Cur.x < EditLines[w.Cur.y].size()
        && !isalnum(EditLines[w.Cur.y][w.Cur.x].GetCh()) );
}

void Editor::Act_ToggleInsert(std::size_t which_window)
{
    auto& w = windows[which_window];
    w.InsertMode = !w.InsertMode;
    Act_Refresh(true);
}

void Editor::Act_HomeHome(std::size_t which_window) // Goto beginning of file
{
    auto& w = windows[which_window];
    w.Cur = w.Win = Anchor();
}

void Editor::Act_EndEnd(std::size_t which_window) // Goto end of file
{
    auto& w = windows[which_window];
    w.Cur.y = EditLines.size()-1;
    w.Win.y = 0;
    if(w.Cur.y >= w.Win.y+w.Dim.y) w.Win.y = w.Cur.y - w.Dim.y + 1;
}

void Editor::Act_PageHome(std::size_t which_window) // Goto beginning of current page
{
    auto& w = windows[which_window];
    w.Cur.y = w.Win.y;
}

void Editor::Act_PageEnd(std::size_t which_window) // Goto end of current page
{
    auto& w = windows[which_window];
    w.Cur.y = w.Win.y + w.Dim.y-1;
}

void Editor::Act_DeleteCurChar(std::size_t which_window) // delete
{
    auto& w = windows[which_window];
    std::size_t eol_x = EditLines[w.Cur.y].size();
    if(eol_x > 0 && EditLines[w.Cur.y][eol_x-1].GetCh() == InternalNewline) --eol_x;
    if(w.Cur.x > eol_x) { w.Cur.x = eol_x; return; } // Just do an end-key
    std::vector<Cell> empty;
    PerformEdit(w.Cur.x, w.Cur.y, 1u, empty);
    UndoQueue.UndoAppendOk = true;
}

void Editor::Act_DeletePrevChar(std::size_t which_window) // backspace (left+delete)
{
    auto& w = windows[which_window];
    std::vector<Cell> empty;
    std::size_t nspaces = 0;
    while(nspaces < EditLines[w.Cur.y].size()
       && EditLines[w.Cur.y][nspaces].GetCh() == U' ') ++nspaces;
    if(nspaces > 0 && w.Cur.x == nspaces)
    {
        // Delete one indent step
        nspaces = 1 + (w.Cur.x-1) % IndentTabSize;
        w.Cur.x -= nspaces;
        PerformEdit(w.Cur.x, w.Cur.y, nspaces, empty);
    }
    else
    {
        Act_Left(which_window);
        Act_DeleteCurChar(which_window);
    }
    UndoQueue.UndoAppendOk = true;
}


void Editor::Act_DeleteCurLine(std::size_t which_window)
{
    auto& w = windows[which_window];
    w.Cur.x = 0;
    std::vector<Cell> empty;
    PerformEdit(w.Cur.x, w.Cur.y, EditLines[w.Cur.y].size(), empty);
}

void Editor::Act_Tab(std::size_t which_window)
{
    // Tab: Insert spaces until next indent stop
    auto& w = windows[which_window];
    std::size_t nspaces = IndentTabSize - w.Cur.x % IndentTabSize;
    std::vector<Cell> txtbuf(nspaces, Cell{U' ', BlankColor});
    PerformEdit(w.Cur.x, w.Cur.y, w.InsertMode?0u:nspaces, txtbuf);
}

void Editor::Act_NewLine(std::size_t which_window)
{
    auto& w = windows[which_window];
    std::size_t nspaces = 0;
    if(w.InsertMode)
    {
        // Autoindent only in insert mode
        std::size_t nspaces = 0;
        while(nspaces < EditLines[w.Cur.y].size()
           && EditLines[w.Cur.y][nspaces].GetCh() == U' ') ++nspaces;
        if(w.Cur.x < nspaces) nspaces = w.Cur.x;
    }
    std::vector<Cell> txtbuf(nspaces+1, Cell{U' ', BlankColor});
    txtbuf[0].SetCh(InternalNewline);
    PerformEdit(w.Cur.x, w.Cur.y, w.InsertMode?0u:1u, txtbuf);
    //w.Win.x = 0;
    UndoQueue.UndoAppendOk = true;
}

void Editor::Act_InsertCharacter(std::size_t which_window, char32_t ch)
{
    auto& w = windows[which_window];
    std::vector<Cell> txtbuf(1, Cell{ch, BlankColor});
    PerformEdit(w.Cur.x, w.Cur.y, w.InsertMode?0u:1u, txtbuf);
    UndoQueue.UndoAppendOk = true;
}

// TODO: Exit
// TODO: Font & resize functoins

void Editor::Act_Refresh(bool cursor_only)
{
    // TODO
}

void Editor::Act_LineAskGo(std::size_t which_window)
{
    auto& w = windows[which_window];

    char Buf[64];
    std::sprintf(Buf, "%lu", (unsigned long) w.Cur.y + 1);

    auto p = PromptText(Printf(U"Goto line:", Buf));
    Act_Refresh();
    if(!p.second) return;
    long l = std::strtol(p.first.c_str(), nullptr, 10) - 1;
    std::size_t oldy = w.Cur.y;
    w.Cur.x = 0;
    w.Cur.y = l>=0 ? l : 0;
    if(w.Cur.y >= EditLines.size()) w.Cur.y = EditLines.size()-1;
    if(w.Win.y > w.Cur.y || w.Win.y + w.Dim.y <= w.Cur.y)
    {
        w.Win.y = w.Cur.y > oldy
            ? (w.Cur.y > (w.Dim.y/2)
                ? w.Cur.y - (w.Dim.y/2)
                : 0)
            : w.Cur.y;
    }
    w.Win.x = 0;
    Act_Refresh();
}

void Editor::Act_LoadAsk(std::size_t which_window)
{
    if(Act_AbandonAsk("FILE OPEN"))
    {
        auto p = PromptText(Printf(U"Load what:", CurrentFilename));
        Act_Refresh();
        if(!p.second) return;

        if(windows.size() == 1)
        {
            FileLoad(p.first.c_str());
        }
        else
        {
            Window w = windows[which_window]; // Make copy
            Editor new_editor;
            if(new_editor.FileLoad(p.first.c_str()))
            {
                // If the new editor successfully loaded this file,
                // close the window from this editor, and assign
                // it to the new one.
                Act_CloseWindow(which_window);
                new_editor.windows.push_back(w);
                // TODO: assign this editor to the global list of editors
            }
        }
    }
}

void Editor::Act_NewAsk(std::size_t which_window)
{
    if(Act_AbandonAsk("START ANEW"))
    {
        if(windows.size() == 1)
            FileNew();
        else
        {
            Window w = windows[which_window]; // Make copy
            Editor new_editor;
            new_editor.FileNew();
            // Close the window from this editor, and assign
            // it to the new one.
            Act_CloseWindow(which_window);
            new_editor.windows.push_back(w);
            // TODO: assign this editor to the global list of editors
        }
    }
}

void Editor::Act_SaveAsk(bool ask_always)
{
    if(ask_always || CurrentFilename.empty())
    {
        auto p = PromptText(Printf(U"Save to:", CurrentFilename));
        Act_Refresh();
        if(!p.second) return;
        FileSave(p.first.c_str());
    }
    else
        FileSave(CurrentFilename.c_str());
}

bool Editor::Act_AbandonAsk(const std::string& action)
{
    if(!UnsavedChanges) return true;
    auto p = PromptText(Printf(U"FILE IS UNSAVED. PROCEED WITH %s? Y/N ", action));
    Act_Refresh();
    if(!p.second) return false;
    // TODO: Change into StatusPrintf and wait for just one key press without enter.
    return p.first == "Y" || p.first == "y";
}

void Editor::Act_SplitWindow(std::size_t which_window, bool sidebyside)
{
    Window& w1 = windows[which_window];
    Window  w2 ( w1 );
    if(sidebyside)
    {
        // Left-Right
        w1.Dim.x /= 2;
        // Leave one character-cell of margin
        w2.Dim.x -= (w1.Dim.x+1);
        w2.Origin.x = w1.Origin.x + w1.Dim.x + 1;
    }
    else
    {
        // Top-bottom
        w1.Dim.y /= 2;
        // Leave one character-cell of margin
        w2.Dim.y -= (w1.Dim.y+1);
        w2.Origin.y = w1.Origin.y + w1.Dim.y + 1;
    }
    windows.insert(windows.begin()+which_window+1, w2);
}

void Editor::Act_CloseWindow(std::size_t which_window)
{
    const Window& w = windows[which_window];
}

void Editor::Act_ReloadSyntaxColor(const std::string& jsf_filename)
{
    // Load new syntax file
    SyntaxColorer.Parse(jsf_filename);
    // Mark the syntax dirty for all lines, so it can be recalculated.
    for(auto& e: EditLines) e.dirtiness |= EditLine::dirty_syntax;
}

std::pair<std::string, bool>
    Editor::PromptText(const std::basic_string<char32_t>& prompt) const
{
    // TODO
}
