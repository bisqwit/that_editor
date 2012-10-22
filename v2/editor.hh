#include <vector>
#include <string>

#include "jsf.hh"
#include "printf.hh"
#include "attr.hh"

class Editor
{
    /*** Fundamental data type ***/
    class Cell
    {
        struct
        {
            char32_t ch;
            AttrType attr;
        } data;
    public:
        Cell(char32_t c=U' ', AttrType attr=UnknownColor) : data{c,attr} {}

        char32_t GetCh()   const { return data.ch;   }
        AttrType GetAttr() const { return data.attr; }
        void SetCh(char32_t ch)  { data.ch   = ch;   }
        void SetAttr(AttrType a) { data.attr = a; }
    };
    struct EditLine: public std::vector<Cell>
    {
        // state of the syntax highlighting after line
        enum: unsigned char { dirty_syntax=1, dirty_cell=2, dirty_quick=4 };
        unsigned char     dirtiness    = dirty_syntax | dirty_cell;
        JSF::ApplyState   syntax_state;

        EditLine() {}
        EditLine(std::size_t n, Cell c) : std::vector<Cell>(n,c) {}

        void MarkChanged()
        {
            dirtiness = (dirtiness | dirty_syntax | dirty_cell) & ~dirty_quick;
        }
    };
    std::vector<EditLine> EditLines;

    /*** File I/O ***/

    std::string CurrentFilename, CurrentFilenameWithoutPath;
    std::size_t chars_file    = 0;
    unsigned    IndentTabSize = 4;
    unsigned    FileTabSize   = 8;

    enum class InputLineStyles { ReadCR, ReadLF, ReadCRLF, ReadBoth } InputLineStyle = InputLineStyles::ReadBoth;
    enum class OutputLineStyles { WriteCR, WriteLF, WriteCRLF }      OutputLineStyle = OutputLineStyles::WriteCRLF;
    bool InputPreserveTabs = false;
    bool OutputCreateTabs  = false;
    char32_t InternalNewline = U'\n';

    void FileNew();
    void FileLoad(const char* fn);
    void FileSave(const char* fn);
    void FileCountCharacters();

    /*** Undo and redo ***/

    struct UndoEvent
    {
        std::size_t x, y;
        std::size_t n_delete;
        std::vector<Cell> insert_chars;
    };
    struct UndoQueueType
    {
        std::list<UndoEvent> UndoQueue, RedoQueue;
        bool UndoAppendOk;

        void AddUndo(const UndoEvent& e);
        void AddRedo(const UndoEvent& e);
        void ClearRedo() { RedoQueue.clear(); }
        void ClearUndo() { UndoQueue.clear(); }
    } UndoQueue;

    void TryUndo();
    void TryRedo();

    /*** Cursors and windows ***/

    struct Anchor
    {
        std::size_t x = 0;
        std::size_t y = 0;
    };

    Anchor BlockBegin, BlockEnd; // Location of block begin&end in the file (global)

    // Return an array containing all cells from the selected area,
    // including newlines where relevant.
    std::vector<Cell> GetSelectedBlock() const;

    // All windows sharing the same edit buffer
    struct Window
    {
        Anchor Win; // Location of window top-left in file
        Anchor Cur; // Location of cursor in the file
        // Edit settings (can be different in each window)
        bool   InsertMode;
        // Window dimensions
        struct
        {
            std::size_t x;
            std::size_t y;
        } Dim;

        // Ensure that the cursor is visible within the window
        void AdjustCursor(bool center = false);
    };
    std::vector<Window> windows;

    template<typename T>
    void ForAllCursors(T&& code, bool IncludingBlockMarkers = true)
    {
        for(auto& w: windows)
        {
            code(w.Win);
            code(w.Cur);
        }
        if(IncludingBlockMarkers)
        {
            code(BlockBegin);
            code(BlockEnd);
        }
    }

    /*** The status line ***/

    ////////////
    // Number of characters in the file
    static constexpr char32_t DefaultStatusLine[] = U"Ad-hoc programming editor - (C) 2012-10-22 Joel Yliluoma";

    std::basic_string<char32_t> StatusLine = DefaultStatusLine;
    bool        UnsavedChanges  = false;
    char        WaitingCtrl   = 0;
    std::size_t chars_typed   = 0;

    void StatusReset()           { StatusLine = DefaultStatusLine; }
    void StatusClear()           { StatusLine.clear(); }

    template<typename... T>
    void StatusPrintf(T... args) { StatusLine = Printf(args...); }

    // Each window has their own top-status bar.
    // The bottom-status bar is global to the editor.
    // The vector must be preinitialized to window width.
    void StatusRenderBottom(std::vector<Cell>& bottom);
    void StatusRenderTop(std::vector<Cell>& top, std::size_t which_window);

    /*** Syntax coloring ***/

    JSF SyntaxColorer;

    // Are there possibly incorrectly colored lines remaining?
    // Return value: First dirty line.
    // Out of range = Nope, everything clean.
    std::size_t SyntaxColorIncomplete() const;

    // Update the coloring for the next line needing fixing.
    // If a window number is given, priority is given to lines
    // within that window if they look horrendous.
    void UpdateSyntaxColor(std::size_t which_window = ~std::size_t(0));

    // Update the coloring for this particular line.
    // Do not call directly. It is a helper function for UpdateSyntaxColor().
    void UpdateSyntaxColorThatLine(std::size_t which_line);

    //////////////////////

    void BlockIndent(int offset);

    void PerformEdit(
        std::size_t x, std::size_t y,
        std::size_t n_delete,
        std::vector<Cell> insert_chars,
        unsigned char DoingUndo = 0);

    // Put the cursor to the corresponding parenthesis
    void FindPair(std::size_t which_window);
};
