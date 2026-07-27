#pragma once

// =============================================================================
// OpenTUI.hpp - TermOx-Style Reactive C++ Widget & Layout Engine
// =============================================================================
// Zero-Dependency, Cross-Platform ANSI Virtual Terminal User Interface Engine
// Inspired by TermOx (CPPurses) Widget & Layout Architecture.
// =============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace OpenTUI {

// =============================================================================
// ANSI Color Palette & Styling System
// =============================================================================
namespace Color {
    inline const std::string Reset       = "\033[0m";
    inline const std::string Bold        = "\033[1m";
    inline const std::string Dim         = "\033[2m";
    inline const std::string Underline   = "\033[4m";
    inline const std::string Reverse     = "\033[7m";

    inline const std::string Black       = "\033[30m";
    inline const std::string Red         = "\033[31m";
    inline const std::string Green       = "\033[32m";
    inline const std::string Yellow      = "\033[33m";
    inline const std::string Blue        = "\033[34m";
    inline const std::string Magenta     = "\033[35m";
    inline const std::string Cyan        = "\033[36m";
    inline const std::string White       = "\033[37m";
    
    inline const std::string BrightCyan  = "\033[96m";
    inline const std::string BrightGreen = "\033[92m";
    inline const std::string BrightWhite = "\033[97m";
    inline const std::string BrightYellow= "\033[93m";
    inline const std::string BrightRed   = "\033[91m";
    inline const std::string BrightBlue  = "\033[94m";
    
    inline const std::string BgBlue      = "\033[44m";
    inline const std::string BgCyan      = "\033[46m";
    inline const std::string BgDarkGray  = "\033[100m";
    inline const std::string BgBlack     = "\033[40m";
}

enum class Key {
    Unknown,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Tab,
    Char
};

struct KeyEvent {
    Key key = Key::Unknown;
    char ch = 0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

// =============================================================================
// Low-Level Terminal Engine & ANSI Control
// =============================================================================
class TerminalEngine {
public:
    static void EnableVirtualTerminal() {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
#endif
    }

    static void MoveCursorToHome() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            COORD coord = { 0, 0 };
            SetConsoleCursorPosition(hOut, coord);
        }
#endif
        std::cout << "\033[H\033[1;1H" << std::flush;
    }

    static void ClearScreen() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            COORD coord = { 0, 0 };
            SetConsoleCursorPosition(hOut, coord);
            DWORD count;
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
                DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
                FillConsoleOutputCharacterA(hOut, ' ', cells, coord, &count);
                FillConsoleOutputAttribute(hOut, csbi.wAttributes, cells, coord, &count);
                SetConsoleCursorPosition(hOut, coord);
            }
        }
        std::cout << "\033[2J\033[1;1H" << std::flush;
#else
        std::cout << "\033[2J\033[1;1H" << std::flush;
#endif
    }

    static void HideCursor() { std::cout << "\033[?25l" << std::flush; }
    static void ShowCursor() { std::cout << "\033[?25h" << std::flush; }

    static KeyEvent ReadKey() {
        KeyEvent event;
#ifdef _WIN32
        int c = _getch();
        if (c == 0 || c == 224) {
            int arrow = _getch();
            switch (arrow) {
                case 72: event.key = Key::Up; break;
                case 80: event.key = Key::Down; break;
                case 75: event.key = Key::Left; break;
                case 77: event.key = Key::Right; break;
            }
        } else if (c == 13) {
            event.key = Key::Enter;
        } else if (c == 27) {
            event.key = Key::Escape;
        } else if (c == 9) {
            event.key = Key::Tab;
        } else {
            event.key = Key::Char;
            event.ch = static_cast<char>(c);
        }
#else
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        char c = 0;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 27) {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        switch (seq[1]) {
                            case 'A': event.key = Key::Up; break;
                            case 'B': event.key = Key::Down; break;
                            case 'C': event.key = Key::Right; break;
                            case 'D': event.key = Key::Left; break;
                        }
                    }
                } else {
                    event.key = Key::Escape;
                }
            } else if (c == 10 || c == 13) {
                event.key = Key::Enter;
            } else if (c == 9) {
                event.key = Key::Tab;
            } else {
                event.key = Key::Char;
                event.ch = c;
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
        return event;
    }
};

// =============================================================================
// Live Spinner Animation
// =============================================================================
class SpinnerAnimation {
    size_t frameIndex = 0;
    std::vector<std::string> frames = {
        "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
    };
public:
    std::string GetNextFrame() {
        std::string f = frames[frameIndex % frames.size()];
        frameIndex++;
        return Color::BrightCyan + f + Color::Reset;
    }
};

// =============================================================================
// TUI Themes (OpenTUI, TermOx, FTXUI)
// =============================================================================
struct ThemeStyle {
    std::string name;
    std::string primaryColor;
    std::string secondaryColor;
    std::string headerColor;
    std::string accentColor;
    std::string statusColor;
    std::string bgHighlight;
    std::string borderTL, borderTR, borderBL, borderBR;
    std::string borderHoriz, borderVert;
    std::string borderSplitL, borderSplitR;
    std::string selectorIcon;
    std::string bannerSubtitle;
};

inline ThemeStyle GetThemeStyle(const std::string& themeName) {
    if (themeName == "TermOx") {
        return {
            "TermOx Widget Engine",
            Color::BrightCyan + Color::Bold,
            "\033[1;95m",                      // Electric Magenta
            "\033[1;95m",                      // Magenta Header
            "\033[1;94m",                      // Deep Blue Accent
            "\033[1;92m",                      // Green Status
            "\033[45m\033[1;97m",              // Magenta Background Highlight
            "╭", "╮", "╰", "╯",                // Smooth Rounded Corners
            "─", "│", "├", "┤",
            "◆ ",
            "[ TERMOX RECT WIDGET ENGINE | C++20 LAYOUT CONTAINER ]"
        };
    } else if (themeName == "FTXUI") {
        return {
            "FTXUI Graphical DOM Engine",
            Color::BrightYellow + Color::Bold, // Gold
            "\033[1;35m",                      // Royal Purple
            "\033[1;35m",                      // Purple Header
            "\033[1;93m",                      // Amber Accent
            "\033[1;96m",                      // Cyan Status
            "\033[43m\033[1;30m",              // Gold Background Highlight
            "┏", "┓", "┗", "┛",                // Heavy Block Borders
            "━", "┃", "┣", "┫",
            "▶ ",
            "[ FTXUI GRAPHICAL DOM ENGINE | COMPONENT TREE RENDERER ]"
        };
    } else { // OpenTUI
        return {
            "OpenTUI Native Engine",
            Color::BrightCyan + Color::Bold,
            Color::BrightCyan,
            Color::BrightWhite + Color::Bold,
            Color::BrightGreen,
            Color::BrightGreen,
            Color::BgBlue + Color::BrightWhite + Color::Bold,
            "╔", "╗", "╚", "╝",                // Unicode Double Lines
            "═", "║", "╠", "╣",
            "► ",
            "[ AUTONOMOUS REACT AGENT | C++17 OPENTUI | AQL ENGINE ]"
        };
    }
}

// =============================================================================
// Extended ASCII & Unicode Double-Box Utilities
// =============================================================================
class Box {
public:
    static std::string DrawBorder(int width, const std::string& title = "", const std::string& themeName = "OpenTUI") {
        auto style = GetThemeStyle(themeName);
        std::ostringstream ss;
        ss << style.secondaryColor << style.borderTL;
        int titleLen = static_cast<int>(title.length());
        int lineLen = width - 2;
        if (titleLen > 0 && titleLen < lineLen - 4) {
            ss << style.borderHoriz << style.borderHoriz << style.borderHoriz
               << "[ " << style.headerColor << title << Color::Reset << style.secondaryColor << " ]";
            int rem = lineLen - titleLen - 7;
            for (int i = 0; i < rem; ++i) ss << style.borderHoriz;
        } else {
            for (int i = 0; i < lineLen; ++i) ss << style.borderHoriz;
        }
        ss << style.borderTR << Color::Reset << "\n";
        return ss.str();
    }

    static std::string DrawDivider(int width, const std::string& themeName = "OpenTUI") {
        auto style = GetThemeStyle(themeName);
        std::ostringstream ss;
        ss << style.secondaryColor << style.borderSplitL;
        for (int i = 0; i < width - 2; ++i) ss << style.borderHoriz;
        ss << style.borderSplitR << Color::Reset << "\n";
        return ss.str();
    }

    static std::string DrawFooter(int width, const std::string& themeName = "OpenTUI") {
        auto style = GetThemeStyle(themeName);
        std::ostringstream ss;
        ss << style.secondaryColor << style.borderBL;
        for (int i = 0; i < width - 2; ++i) ss << style.borderHoriz;
        ss << style.borderBR << Color::Reset << "\n";
        return ss.str();
    }

    static int GetVisibleDisplayWidth(const std::string& str) {
        int width = 0;
        size_t i = 0;
        while (i < str.length()) {
            if (str[i] == '\033') {
                while (i < str.length() && str[i] != 'm') i++;
                if (i < str.length()) i++;
            } else {
                unsigned char c1 = static_cast<unsigned char>(str[i]);
                if (c1 < 0x80) {
                    width += 1; i += 1;
                } else if ((c1 & 0xE0) == 0xC0) {
                    width += 1; i += 2;
                } else if ((c1 & 0xF0) == 0xE0) {
                    if (i + 2 < str.length()) {
                        unsigned char c2 = static_cast<unsigned char>(str[i+1]);
                        unsigned char c3 = static_cast<unsigned char>(str[i+2]);
                        uint32_t codepoint = ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                        if (codepoint == 0x26AA || codepoint == 0x26AB || codepoint == 0x26BD || codepoint == 0x26BE || (codepoint >= 0x2600 && codepoint <= 0x26FF)) {
                            width += 2;
                        } else {
                            width += 1;
                        }
                    } else {
                        width += 1;
                    }
                    i += 3;
                } else if ((c1 & 0xF8) == 0xF0) {
                    width += 2; i += 4;
                } else {
                    i += 1;
                }
            }
        }
        return width;
    }

    static std::string TruncateVisibleText(const std::string& str, int maxVisible) {
        int curWidth = GetVisibleDisplayWidth(str);
        if (curWidth <= maxVisible) return str;

        std::string res;
        int width = 0;
        size_t i = 0;
        while (i < str.length() && width < maxVisible - 3) {
            if (str[i] == '\033') {
                while (i < str.length() && str[i] != 'm') res += str[i++];
                if (i < str.length()) res += str[i++];
            } else {
                unsigned char c1 = static_cast<unsigned char>(str[i]);
                int charWidth = 1;
                size_t charBytes = 1;
                if (c1 < 0x80) { charWidth = 1; charBytes = 1; }
                else if ((c1 & 0xE0) == 0xC0) { charWidth = 1; charBytes = 2; }
                else if ((c1 & 0xF0) == 0xE0) {
                    if (i + 2 < str.length()) {
                        unsigned char c2 = static_cast<unsigned char>(str[i+1]);
                        unsigned char c3 = static_cast<unsigned char>(str[i+2]);
                        uint32_t codepoint = ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                        if (codepoint == 0x26AA || codepoint == 0x26AB || (codepoint >= 0x2600 && codepoint <= 0x26FF)) {
                            charWidth = 2;
                        }
                    }
                    charBytes = 3;
                } else if ((c1 & 0xF8) == 0xF0) {
                    charWidth = 2; charBytes = 4;
                }

                if (width + charWidth > maxVisible - 3) break;
                for (size_t b = 0; b < charBytes && i < str.length(); ++b) res += str[i++];
                width += charWidth;
            }
        }
        res += "...";
        return res;
    }

    static std::string DrawLine(int width, const std::string& text, bool highlight = false, const std::string& themeName = "OpenTUI") {
        auto style = GetThemeStyle(themeName);
        std::ostringstream ss;
        ss << style.secondaryColor << style.borderVert << " " << Color::Reset;
        std::string safeText = TruncateVisibleText(text, width - 7);
        int visibleWidth = GetVisibleDisplayWidth(safeText);
        if (highlight) {
            ss << style.bgHighlight << " " << style.selectorIcon << safeText;
            int padding = width - visibleWidth - 7;
            int fill = (std::max)(0, padding);
            for (int i = 0; i < fill; ++i) ss << " ";
            ss << Color::Reset;
        } else {
            ss << "   " << safeText;
            int padding = width - visibleWidth - 7;
            int fill = (std::max)(0, padding);
            for (int i = 0; i < fill; ++i) ss << " ";
        }
        ss << style.secondaryColor << " " << style.borderVert << Color::Reset << "\n";
        return ss.str();
    }
};

// =============================================================================
// Loading & Progress Bar Widget Utility
// =============================================================================
class ProgressBar {
public:
    static std::string Render(double percentage, int width = 30, const std::string& unitLabel = "%") {
        percentage = (std::min)((std::max)(percentage, 0.0), 100.0);
        int filled = static_cast<int>((percentage / 100.0) * width);
        
        std::ostringstream ss;
        ss << Color::BrightCyan << "▕" << Color::BrightGreen;
        for (int i = 0; i < filled; ++i) ss << "█";
        ss << Color::Dim;
        for (int i = filled; i < width; ++i) ss << "░";
        ss << Color::Reset << Color::BrightCyan << "▏ " << Color::BrightWhite << Color::Bold
           << std::fixed << std::setprecision(1) << percentage << unitLabel << Color::Reset;
        return ss.str();
    }
};

// =============================================================================
// TermOx-Style Reactive Widget Base Class
// =============================================================================
class Widget {
protected:
    Rect bounds;
    bool visible = true;
    bool focused = false;
    std::string name;

public:
    Widget(const std::string& widgetName = "Widget") : name(widgetName) {}
    virtual ~Widget() = default;

    void SetBounds(const Rect& r) { bounds = r; }
    Rect GetBounds() const { return bounds; }

    void SetVisible(bool v) { visible = v; }
    bool IsVisible() const { return visible; }

    void SetFocus(bool f) { focused = f; }
    bool IsFocused() const { return focused; }

    std::string GetName() const { return name; }

    virtual void Render(std::ostringstream& ss) = 0;
    virtual bool OnKeyEvent(const KeyEvent& ev) { (void)ev; return false; }
};

// =============================================================================
// TermOx-Style Container Panel Widget (Titled Border Box)
// =============================================================================
class PanelWidget : public Widget {
    std::string title;
    std::vector<std::string> lines;

public:
    PanelWidget(const std::string& panelTitle = "")
        : Widget("Panel"), title(panelTitle) {}

    void SetTitle(const std::string& t) { title = t; }
    void SetLines(const std::vector<std::string>& l) { lines = l; }
    void AddLine(const std::string& l) { lines.push_back(l); }
    void Clear() { lines.clear(); }

    void Render(std::ostringstream& ss) override {
        if (!visible) return;
        int w = bounds.width > 0 ? bounds.width : 80;
        ss << Box::DrawBorder(w, title);
        for (const auto& l : lines) {
            ss << Box::DrawLine(w, l, false);
        }
        ss << Box::DrawFooter(w);
    }
};

// =============================================================================
// TermOx-Style Interactive Menu Selection Component
// =============================================================================
struct MenuSelection {
    int index = -1;
    Key actionKey = Key::Enter;
};

class Menu {
    std::string title;
    std::vector<std::string> options;
    int selectedIndex = 0;
    std::string statusLine;
    std::vector<std::string> headerLines;
    std::string themeName = "OpenTUI";
    SpinnerAnimation spinner;
    std::function<void()> onPreRender = nullptr;

public:
    Menu(const std::string& t, const std::vector<std::string>& opts, const std::string& theme = "OpenTUI")
        : title(t), options(opts), themeName(theme) {}

    void SetTheme(const std::string& t) { themeName = t; }
    std::string GetTheme() const { return themeName; }
    void SetPreRenderCallback(std::function<void()> cb) { onPreRender = cb; }
    void SetStatusLine(const std::string& status) { statusLine = status; }
    void SetHeaderLines(const std::vector<std::string>& headers) { headerLines = headers; }
    int GetSelectedIndex() const { return selectedIndex; }

    int Show() {
        return ShowExtended().index;
    }

    MenuSelection ShowExtended() {
        TerminalEngine::EnableVirtualTerminal();
        TerminalEngine::HideCursor();
        TerminalEngine::ClearScreen();

        while (true) {
            std::ostringstream frame;

            if (onPreRender) {
                auto* oldBuf = std::cout.rdbuf(frame.rdbuf());
                onPreRender();
                std::cout.rdbuf(oldBuf);
            }

            frame << Box::DrawBorder(80, title, themeName);

            if (!headerLines.empty()) {
                for (const auto& h : headerLines) {
                    frame << Box::DrawLine(80, h, false, themeName);
                }
                frame << Box::DrawDivider(80, themeName);
            }

            for (size_t i = 0; i < options.size(); ++i) {
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                frame << Box::DrawLine(80, options[i], isSelected, themeName);
            }

            if (!statusLine.empty()) {
                frame << Box::DrawDivider(80, themeName);
                std::string animatedStatus = spinner.GetNextFrame() + " " + statusLine;
                frame << Box::DrawLine(80, animatedStatus, false, themeName);
            }

            frame << Box::DrawFooter(80, themeName);
            frame << "\033[90m Use UP/DOWN to navigate, LEFT/RIGHT or Enter to toggle/select, ESC/'q' to exit.\033[0m\n";

            TerminalEngine::MoveCursorToHome();
            std::cout << frame.str() << "\033[J" << std::flush;

            KeyEvent ev = TerminalEngine::ReadKey();
            if (ev.key == Key::Up) {
                selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : static_cast<int>(options.size()) - 1;
            } else if (ev.key == Key::Down) {
                selectedIndex = (selectedIndex + 1) % static_cast<int>(options.size());
            } else if (ev.key == Key::Left || ev.key == Key::Right) {
                TerminalEngine::ShowCursor();
                return { selectedIndex, ev.key };
            } else if (ev.key == Key::Enter) {
                TerminalEngine::ShowCursor();
                return { selectedIndex, Key::Enter };
            } else if (ev.key == Key::Escape || (ev.key == Key::Char && (ev.ch == 'q' || ev.ch == 'Q'))) {
                TerminalEngine::ShowCursor();
                return { -1, Key::Escape };
            }
        }
    }
};

// =============================================================================
// TermOx-Style Text Prompt & Input Component
// =============================================================================
class TextInput {
public:
    static std::string ReadLine(const std::string& promptStr, const std::string& defaultVal = "", const std::vector<std::string>& suggestions = {}) {
        TerminalEngine::EnableVirtualTerminal();
        TerminalEngine::ShowCursor();

        std::string input = "";
        std::string ghost = "";

        auto updateDisplay = [&](const std::string& newInput) {
            std::cout << "\r\033[K" << Color::BrightCyan << Color::Bold << promptStr << Color::Reset << newInput;

            ghost = "";
            if (!newInput.empty() && !suggestions.empty()) {
                std::string inputLower = newInput;
                std::transform(inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower);
                for (const auto& sug : suggestions) {
                    std::string sugLower = sug;
                    std::transform(sugLower.begin(), sugLower.end(), sugLower.begin(), ::tolower);
                    if (sugLower.find(inputLower) == 0 && sug.length() > newInput.length()) {
                        ghost = sug.substr(newInput.length());
                        std::cout << Color::Dim << ghost << Color::Reset;
                        for (size_t g = 0; g < ghost.length(); ++g) std::cout << "\b";
                        break;
                    }
                }
            }
            std::cout << std::flush;
        };

        std::cout << Color::BrightCyan << Color::Bold << promptStr << Color::Reset << std::flush;

        while (true) {
            KeyEvent ev = TerminalEngine::ReadKey();
            if (ev.key == Key::Enter) {
                std::cout << "\n";
                if (input.empty()) return defaultVal;
                return input;
            } else if (ev.key == Key::Escape) {
                std::cout << "\n";
                return "";
            } else if (ev.key == Key::Char) {
                if (ev.ch == 9) { // TAB key autocompletes ghost text!
                    if (!ghost.empty()) {
                        input += ghost;
                        updateDisplay(input);
                    }
                } else if (ev.ch == 8 || ev.ch == 127) { // Backspace
                    if (!input.empty()) {
                        input.pop_back();
                        updateDisplay(input);
                    }
                } else if (ev.ch >= 32 && ev.ch <= 126) { // Printable characters
                    input.push_back(ev.ch);
                    updateDisplay(input);
                }
            }
        }
    }
};

} // namespace OpenTUI
