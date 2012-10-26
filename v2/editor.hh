#include <vector>
#include <string>

#include "syntax.hh"
#include "printf.hh"
#include "attr.hh"

#define DefaultStatusLine \
    U"Ad-hoc programming editor - (C) 2012-10-22 Joel Yliluoma"

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

    enum class InputLineStyles  { ReadCR, ReadLF, ReadCRLF, ReadBoth } InputLineStyle = InputLineStyles::ReadBoth;
    enum class OutputLineStyles { WriteCR=1, WriteLF=2, WriteCRLF=3 }      OutputLineStyle = OutputLineStyles::WriteCRLF;
    bool InputPreserveTabs = false;
    bool OutputCreateTabs  = false;
    char32_t InternalNewline = U'\n';

    void FileNew();
    bool FileLoad(const char* fn);
    bool FileSave(const char* fn);
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
        void ClearUndo() { UndoQueue.clear(); UndoAppendOk = false; }
    } UndoQueue;

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
        } Dim, Origin;

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

    void PerformEdit(
        std::size_t x, std::size_t y,
        std::size_t n_delete,
        const std::vector<Cell>& insert_chars,
        unsigned char DoingUndo = 0);

    void Act_BlockIndent(int offset);

    // Put the cursor to the corresponding parenthesis
    void Act_FindPair(std::size_t which_window);

    void Act_PageDn(std::size_t which_window);
    void Act_PageUp(std::size_t which_window);
    void Act_TryUndo();
    void Act_TryRedo();
    void Act_WindowUp(std::size_t which_window);
    void Act_WindowDn(std::size_t which_window);
    void Act_BlockMark(std::size_t which_window);
    void Act_BlockMarkEnd(std::size_t which_window);
    void Act_BlockMove(std::size_t which_window);
    void Act_BlockCopy(std::size_t which_window);
    void Act_BlockDelete();
    void Act_BlockIndent();
    void Act_BlockUnindent();
    void Act_CharacterInfo(std::size_t which_window);
    void Act_InsertLiteralCharacter(std::size_t which_window, char32_t ch);
    void Act_Up(std::size_t which_window);
    void Act_Dn(std::size_t which_window);
    void Act_Home(std::size_t which_window);
    void Act_End(std::size_t which_window);
    void Act_Left(std::size_t which_window);
    void Act_Right(std::size_t which_window);
    void Act_WordLeft(std::size_t which_window);
    void Act_WordRight(std::size_t which_window);
    void Act_ToggleInsert(std::size_t which_window);
    void Act_HomeHome(std::size_t which_window); // Goto beginning of file
    void Act_EndEnd(std::size_t which_window); // Goto end of file
    void Act_PageHome(std::size_t which_window); // Goto beginning of current page
    void Act_PageEnd(std::size_t which_window); // Goto end of current page
    void Act_DeleteCurChar(std::size_t which_window); // delete
    void Act_DeletePrevChar(std::size_t which_window); // backspace (left+delete);
    void Act_DeleteCurLine(std::size_t which_window);
    void Act_Tab(std::size_t which_window);
    void Act_NewLine(std::size_t which_window);
    void Act_InsertCharacter(std::size_t which_window, char32_t ch);
    void Act_LineAskGo(std::size_t which_window);
    void Act_NewAsk(std::size_t which_window);
    void Act_LoadAsk(std::size_t which_window);
    void Act_SaveAsk(bool ask_always = true);
    bool Act_AbandonAsk(const std::string& action);
    void Act_SplitWindow(std::size_t which_window, bool sidebyside);
    void Act_CloseWindow(std::size_t which_window);
    void Act_ReloadSyntaxColor(const std::string& jsf_filename);

    void Act_Refresh(bool cursor_only = false);

    std::pair<std::string, bool> PromptText(const std::basic_string<char32_t>& prompt) const;
};
