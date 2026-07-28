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
#include <sys/select.h>
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

        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hIn, &dwMode)) {
                dwMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
                dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
                SetConsoleMode(hIn, dwMode);
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

    static void ClearScreen(const std::string& panelBg = "") {
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
#endif
        if (!panelBg.empty()) {
            std::cout << panelBg << "\033[2J\033[1;1H" << std::flush;
        } else {
            std::cout << "\033[2J\033[1;1H" << std::flush;
        }
    }

    static void HideCursor() { std::cout << "\033[?25l" << std::flush; }
    static void ShowCursor() { std::cout << "\033[?25h" << std::flush; }

    static bool HasKeyPending() {
#ifdef _WIN32
        return _kbhit() != 0;
#else
        struct timeval tv = { 0, 0 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
#endif
    }

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
    std::string panelBg;        // Container Panel Background ANSI escape code
    std::string bgHighlight;    // Menu Item Active Background ANSI escape code
    std::string borderTL, borderTR, borderBL, borderBR;
    std::string borderHoriz, borderVert;
    std::string borderSplitL, borderSplitR;
    std::string selectorIcon;
    std::string bannerSubtitle;
};

inline ThemeStyle GetThemeStyle(const std::string& themeName, const std::string& colorScheme = "Default", const std::string& fgOverride = "Default", const std::string& bgOverride = "Default") {
    std::string bTL = "╔", bTR = "╗", bBL = "╚", bBR = "╝", bH = "═", bV = "║", bSL = "╠", bSR = "╣", sel = "► ", banner = "[ AUTONOMOUS REACT AGENT | C++17 OPENTUI | AQL ENGINE ]";
    
    // Default Engine Native Color Palettes (Both Foreground and Background)
    std::string pri = Color::BrightCyan + Color::Bold;
    std::string sec = Color::BrightCyan;
    std::string hdr = Color::BrightWhite + Color::Bold;
    std::string acc = Color::BrightGreen;
    std::string st  = Color::BrightGreen;
    std::string pBg = "\033[44m";                      // Deep Navy Blue Container Background
    std::string bg  = "\033[106m\033[1;30m";          // Bright Cyan Highlight Box, Dark Text

    if (themeName == "TermOx") {
        bTL = "╭"; bTR = "╮"; bBL = "╰"; bBR = "╯"; bH = "─"; bV = "│"; bSL = "├"; bSR = "┤"; sel = "◆ ";
        banner = "[ TERMOX RECT WIDGET ENGINE | C++20 LAYOUT CONTAINER ]";
        // TermOx signature Electric Magenta & Deep Blue theme colors with Magenta Panel Background
        pri = "\033[1;97m"; sec = "\033[1;95m"; hdr = "\033[1;95m"; acc = "\033[1;94m"; st = "\033[1;92m"; 
        pBg = "\033[45m";                              // Electric Magenta Container Background
        bg  = "\033[105m\033[1;30m";                   // Bright Magenta Highlight Box, Dark Text
    } else if (themeName == "FTXUI") {
        bTL = "┏"; bTR = "┓"; bBL = "┗"; bBR = "┛"; bH = "━"; bV = "┃"; bSL = "┣"; bSR = "┫"; sel = "▶ ";
        banner = "[ FTXUI GRAPHICAL DOM ENGINE | COMPONENT TREE RENDERER ]";
        // FTXUI signature Amber Gold & Royal Purple DOM theme colors with Amber Panel Background
        pri = "\033[1;30m"; sec = "\033[1;35m"; hdr = "\033[1;35m"; acc = "\033[1;93m"; st = "\033[1;96m"; 
        pBg = "\033[43m";                              // Amber Gold Container Background
        bg  = "\033[45m\033[1;97m";                    // Royal Purple Highlight Box, White Text
    }

    // Explicit Color Scheme Preset Overrides
    if (colorScheme == "Dracula Dark") {
        pri = "\033[1;38;5;141m"; sec = "\033[38;5;212m"; hdr = "\033[1;38;5;231m"; acc = "\033[38;5;84m"; st = "\033[38;5;84m";
        pBg = "\033[48;5;235m"; bg = "\033[48;5;61m\033[1;38;5;231m";
    } else if (colorScheme == "Tokyo Night") {
        pri = "\033[1;38;5;117m"; sec = "\033[38;5;176m"; hdr = "\033[1;38;5;231m"; acc = "\033[38;5;120m"; st = "\033[38;5;120m";
        pBg = "\033[48;5;234m"; bg = "\033[48;5;62m\033[1;38;5;231m";
    } else if (colorScheme == "Nordic Frost") {
        pri = "\033[1;38;5;111m"; sec = "\033[38;5;153m"; hdr = "\033[1;38;5;231m"; acc = "\033[38;5;117m"; st = "\033[38;5;117m";
        pBg = "\033[48;5;236m"; bg = "\033[48;5;24m\033[1;38;5;231m";
    } else if (colorScheme == "Catppuccin Mocha") {
        pri = "\033[1;38;5;218m"; sec = "\033[38;5;183m"; hdr = "\033[1;38;5;231m"; acc = "\033[38;5;115m"; st = "\033[38;5;115m";
        pBg = "\033[48;5;235m"; bg = "\033[48;5;98m\033[1;38;5;231m";
    } else if (colorScheme == "Cyberpunk 2077") {
        pri = "\033[1;38;5;226m"; sec = "\033[38;5;51m"; hdr = "\033[1;38;5;231m"; acc = "\033[38;5;226m"; st = "\033[38;5;51m";
        pBg = "\033[48;5;233m"; bg = "\033[48;5;220m\033[1;38;5;16m";
    } else if (colorScheme == "Solarized Ocean") {
        pri = "\033[1;38;5;37m"; sec = "\033[38;5;136m"; hdr = "\033[1;38;5;231m"; acc = "\033[38;5;64m"; st = "\033[38;5;37m";
        pBg = "\033[48;5;17m"; bg = "\033[48;5;25m\033[1;38;5;231m";
    } else if (colorScheme == "Cyan Matrix") {
        pri = Color::BrightCyan + Color::Bold; sec = Color::BrightCyan; hdr = Color::BrightWhite + Color::Bold; acc = Color::BrightGreen; st = Color::BrightGreen;
        pBg = "\033[40m"; bg = "\033[46m\033[1;30m";
    } else if (colorScheme == "Electric Magenta") {
        pri = "\033[1;97m"; sec = "\033[1;95m"; hdr = "\033[1;95m"; acc = "\033[1;94m"; st = "\033[1;92m";
        pBg = "\033[45m"; bg = "\033[105m\033[1;30m";
    } else if (colorScheme == "Amber Gold") {
        pri = "\033[1;30m"; sec = "\033[1;35m"; hdr = "\033[1;35m"; acc = "\033[1;33m"; st = "\033[1;96m";
        pBg = "\033[43m"; bg = "\033[45m\033[1;97m";
    } else if (colorScheme == "Emerald Cyber") {
        pri = "\033[1;97m"; sec = "\033[1;92m"; hdr = "\033[1;97m"; acc = "\033[1;36m"; st = "\033[1;92m";
        pBg = "\033[42m"; bg = "\033[102m\033[1;30m";
    } else if (colorScheme == "Neon Cyberpunk") {
        pri = "\033[1;95m"; sec = "\033[1;96m"; hdr = "\033[1;93m"; acc = "\033[1;96m"; st = "\033[1;95m";
        pBg = "\033[46m"; bg = "\033[45m\033[1;97m";
    } else if (colorScheme == "Monochrome Slate") {
        pri = "\033[1;97m"; sec = "\033[1;37m"; hdr = "\033[1;97m"; acc = "\033[1;37m"; st = "\033[1;97m";
        pBg = "\033[100m"; bg = "\033[47m\033[1;30m";
    }

    // Granular Foreground Color Setting Override
    if (fgOverride == "Cyan") pri = sec = "\033[1;96m";
    else if (fgOverride == "Electric Magenta") pri = sec = "\033[1;95m";
    else if (fgOverride == "Amber Gold") pri = sec = "\033[1;93m";
    else if (fgOverride == "Emerald Green") pri = sec = "\033[1;92m";
    else if (fgOverride == "Neon Pink") pri = sec = "\033[1;95m";
    else if (fgOverride == "Bright White") pri = sec = "\033[1;97m";
    else if (fgOverride == "Yellow") pri = sec = "\033[1;33m";
    else if (fgOverride == "Royal Blue") pri = sec = "\033[1;94m";

    // Granular Background Color Setting Override
    if (bgOverride == "Black") pBg = "\033[40m";
    else if (bgOverride == "Dracula Charcoal") pBg = "\033[48;5;235m";
    else if (bgOverride == "Tokyo Night") pBg = "\033[48;5;234m";
    else if (bgOverride == "Nordic Frost") pBg = "\033[48;5;236m";
    else if (bgOverride == "Solarized Ocean") pBg = "\033[48;5;17m";
    else if (bgOverride == "Navy Blue") pBg = "\033[44m";
    else if (bgOverride == "Electric Magenta") pBg = "\033[45m";
    else if (bgOverride == "Amber Gold") pBg = "\033[43m";
    else if (bgOverride == "Emerald Green") pBg = "\033[42m";
    else if (bgOverride == "Dark Slate") pBg = "\033[100m";
    else if (bgOverride == "Charcoal Gray") pBg = "\033[47m";

    return {
        themeName, pri, sec, hdr, acc, st, pBg, bg,
        bTL, bTR, bBL, bBR, bH, bV, bSL, bSR, sel, banner
    };
}

// =============================================================================
// Extended ASCII & Unicode Double-Box Utilities
// =============================================================================
class Box {
public:
    static std::string DrawBorder(int width, const std::string& title = "", const std::string& themeName = "OpenTUI", const std::string& colorScheme = "Default", const std::string& fg = "Default", const std::string& bg = "Default") {
        auto style = GetThemeStyle(themeName, colorScheme, fg, bg);
        std::ostringstream ss;
        ss << style.panelBg << style.secondaryColor << style.borderTL;
        int titleLen = static_cast<int>(title.length());
        int lineLen = width - 2;
        if (titleLen > 0 && titleLen < lineLen - 4) {
            ss << style.borderHoriz << style.borderHoriz << style.borderHoriz
               << "[ " << style.headerColor << title << Color::Reset << style.panelBg << style.secondaryColor << " ]";
            int rem = lineLen - titleLen - 7;
            for (int i = 0; i < rem; ++i) ss << style.borderHoriz;
        } else {
            for (int i = 0; i < lineLen; ++i) ss << style.borderHoriz;
        }
        ss << style.borderTR << style.panelBg << "\033[K\n";
        return ss.str();
    }

    static std::string DrawDivider(int width, const std::string& themeName = "OpenTUI", const std::string& colorScheme = "Default", const std::string& fg = "Default", const std::string& bg = "Default") {
        auto style = GetThemeStyle(themeName, colorScheme, fg, bg);
        std::ostringstream ss;
        ss << style.panelBg << style.secondaryColor << style.borderSplitL;
        for (int i = 0; i < width - 2; ++i) ss << style.borderHoriz;
        ss << style.borderSplitR << style.panelBg << "\033[K\n";
        return ss.str();
    }

    static std::string DrawFooter(int width, const std::string& themeName = "OpenTUI", const std::string& colorScheme = "Default", const std::string& fg = "Default", const std::string& bg = "Default") {
        auto style = GetThemeStyle(themeName, colorScheme, fg, bg);
        std::ostringstream ss;
        ss << style.panelBg << style.secondaryColor << style.borderBL;
        for (int i = 0; i < width - 2; ++i) ss << style.borderHoriz;
        ss << style.borderBR << style.panelBg << "\033[K\n";
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

    static std::string DrawLine(int width, const std::string& text, bool highlight = false, const std::string& themeName = "OpenTUI", const std::string& colorScheme = "Default", const std::string& fg = "Default", const std::string& bg = "Default") {
        auto style = GetThemeStyle(themeName, colorScheme, fg, bg);
        std::ostringstream ss;
        ss << style.panelBg << style.secondaryColor << style.borderVert << " ";
        std::string safeText = TruncateVisibleText(text, width - 7);
        int visibleWidth = GetVisibleDisplayWidth(safeText);
        if (highlight) {
            ss << style.bgHighlight << " " << style.selectorIcon << safeText;
            int padding = width - visibleWidth - 7;
            int fill = (std::max)(0, padding);
            for (int i = 0; i < fill; ++i) ss << " ";
            ss << Color::Reset;
        } else {
            ss << style.panelBg << style.primaryColor << "   " << safeText;
            int padding = width - visibleWidth - 7;
            int fill = (std::max)(0, padding);
            for (int i = 0; i < fill; ++i) ss << " ";
            ss << Color::Reset;
        }
        ss << style.panelBg << style.secondaryColor << " " << style.borderVert << style.panelBg << "\033[K\n";
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

    static bool HasKeyPending() {
#ifdef _WIN32
        return _kbhit() != 0;
#else
        struct timeval tv = { 0, 0 };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
#endif
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
    int refreshIntervalMs = 0;                      // Auto re-render interval in ms (0 = disabled, keypress-only)
    std::chrono::steady_clock::time_point lastRenderTime;
    std::string themeName = "OpenTUI";
    std::string colorScheme = "Default";
    std::string fgColor = "Default";
    std::string bgColor = "Default";
    SpinnerAnimation spinner;
    std::function<void()> onPreRender = nullptr;

public:
    Menu(const std::string& t, const std::vector<std::string>& opts, const std::string& theme = "OpenTUI", const std::string& scheme = "Default", const std::string& fg = "Default", const std::string& bg = "Default")
        : title(t), options(opts), themeName(theme), colorScheme(scheme), fgColor(fg), bgColor(bg) {}

    void SetTheme(const std::string& t) { themeName = t; }
    std::string GetTheme() const { return themeName; }
    void SetColorScheme(const std::string& s) { colorScheme = s; }
    std::string GetColorScheme() const { return colorScheme; }
    void SetFgColor(const std::string& fg) { fgColor = fg; }
    std::string GetFgColor() const { return fgColor; }
    void SetBgColor(const std::string& bg) { bgColor = bg; }
    std::string GetBgColor() const { return bgColor; }
    void SetPreRenderCallback(std::function<void()> cb) { onPreRender = cb; }
    void SetStatusLine(const std::string& status) { statusLine = status; }
    void SetHeaderLines(const std::vector<std::string>& headers) { headerLines = headers; }
    void SetRefreshIntervalMs(int ms) { refreshIntervalMs = ms; }
    void SetSelectedIndex(int idx) {
        if (idx >= 0 && idx < static_cast<int>(options.size())) {
            selectedIndex = idx;
        }
    }
    int GetSelectedIndex() const { return selectedIndex; }

    int Show() {
        return ShowExtended().index;
    }

    MenuSelection ShowExtended() {
        TerminalEngine::EnableVirtualTerminal();
        TerminalEngine::HideCursor();
        TerminalEngine::ClearScreen();

        while (true) {
            // Clear screen on each re-render so live refresh doesn't leave artifacts
            std::cout << "\033[2J\033[H" << std::flush;

            auto style = GetThemeStyle(themeName, colorScheme, fgColor, bgColor);
            std::ostringstream frame;

            if (onPreRender) {
                // Legacy compatibility: still capture pre-render output to frame
                auto* oldBuf = std::cout.rdbuf(frame.rdbuf());
                onPreRender();
                std::cout.rdbuf(oldBuf);
            }
            if (themeName == "TermOx") {
                // =============================================================
                // TERMOX C++20 REACTIVE WIDGET TREE & WINDOW LAYOUT ENGINE
                // =============================================================
                std::string topRibbon = "╭─ [File] ── [Scan] ── [Tools] ── [AQL Console] ── [Settings] ── [Help] ─╮";
                frame << style.secondaryColor << topRibbon << style.panelBg << "\033[K\n";
                frame << Box::DrawBorder(80, title, themeName, colorScheme, fgColor, bgColor);

                if (!headerLines.empty()) {
                    for (const auto& h : headerLines) {
                        frame << Box::DrawLine(80, h, false, themeName, colorScheme, fgColor, bgColor);
                    }
                    frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                }

                for (size_t i = 0; i < options.size(); ++i) {
                    bool isSelected = (static_cast<int>(i) == selectedIndex);
                    frame << Box::DrawLine(80, options[i], isSelected, themeName, colorScheme, fgColor, bgColor);
                }

                if (!statusLine.empty()) {
                    frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                    std::string animatedStatus = spinner.GetNextFrame() + " " + statusLine;
                    frame << Box::DrawLine(80, animatedStatus, false, themeName, colorScheme, fgColor, bgColor);
                }

                frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                std::string navHint = "[TermOx Widget] ↑/↓: Move │ ←/→/Enter: Select/Toggle │ ESC/'q': Exit";
                frame << Box::DrawLine(80, navHint, false, themeName, colorScheme, fgColor, bgColor);
                frame << Box::DrawFooter(80, themeName, colorScheme, fgColor, bgColor);

            } else if (themeName == "FTXUI") {
                // =============================================================
                // FTXUI FUNCTIONAL GRAPHICAL DOM COMPONENT TREE RENDERER
                // =============================================================
                frame << Box::DrawBorder(80, "SYSTEM-CLEANER-AGENT DOM TREE", themeName, colorScheme, fgColor, bgColor);

                if (!headerLines.empty()) {
                    for (const auto& h : headerLines) {
                        frame << Box::DrawLine(80, h, false, themeName, colorScheme, fgColor, bgColor);
                    }
                    frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                }

                for (size_t i = 0; i < options.size(); ++i) {
                    bool isSelected = (static_cast<int>(i) == selectedIndex);
                    frame << Box::DrawLine(80, options[i], isSelected, themeName, colorScheme, fgColor, bgColor);
                }

                if (!statusLine.empty()) {
                    frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                    std::string animatedStatus = spinner.GetNextFrame() + " " + statusLine;
                    frame << Box::DrawLine(80, animatedStatus, false, themeName, colorScheme, fgColor, bgColor);
                }

                frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                std::string tabStrip = "[Tab 1: Dashboard]   [Tab 2: AQL Console]   [Tab 3: Settings]";
                frame << Box::DrawLine(80, tabStrip, false, themeName, colorScheme, fgColor, bgColor);
                frame << Box::DrawFooter(80, themeName, colorScheme, fgColor, bgColor);

            } else {
                // =============================================================
                // OPENTUI CLASSIC REACTIVE DASHBOARD ENGINE
                // =============================================================
                frame << Box::DrawBorder(80, title, themeName, colorScheme, fgColor, bgColor);

                if (!headerLines.empty()) {
                    for (const auto& h : headerLines) {
                        frame << Box::DrawLine(80, h, false, themeName, colorScheme, fgColor, bgColor);
                    }
                    frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                }

                for (size_t i = 0; i < options.size(); ++i) {
                    bool isSelected = (static_cast<int>(i) == selectedIndex);
                    frame << Box::DrawLine(80, options[i], isSelected, themeName, colorScheme, fgColor, bgColor);
                }

                if (!statusLine.empty()) {
                    frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                    std::string animatedStatus = spinner.GetNextFrame() + " " + statusLine;
                    frame << Box::DrawLine(80, animatedStatus, false, themeName, colorScheme, fgColor, bgColor);
                }

                frame << Box::DrawDivider(80, themeName, colorScheme, fgColor, bgColor);
                std::string navHint = "Use UP/DOWN to navigate, LEFT/RIGHT or Enter to toggle/select, ESC/'q' to exit.";
                frame << Box::DrawLine(80, navHint, false, themeName, colorScheme, fgColor, bgColor);

                frame << Box::DrawFooter(80, themeName, colorScheme, fgColor, bgColor);
            }

            frame << "\033[J";
            std::cout << frame.str() << std::flush;
            lastRenderTime = std::chrono::steady_clock::now();

            // -----------------------------------------------------------------
            // Auto-refresh loop: re-render the frame every refreshIntervalMs
            // (if set) so live stats (RAM/Disk) update without user input.
            // We poll _kbhit() / select() with a short timeout and only
            // re-render when the interval has elapsed.
            // -----------------------------------------------------------------
            bool timedOutNoInput = false;
            KeyEvent ev;
            if (refreshIntervalMs > 0) {
                while (true) {
#ifdef _WIN32
                    if (_kbhit()) { ev = TerminalEngine::ReadKey(); break; }
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - lastRenderTime).count();
                    if (elapsed >= refreshIntervalMs) {
                        timedOutNoInput = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#else
                    struct timeval tv;
                    tv.tv_sec = 0;
                    tv.tv_usec = 50000; // 50ms poll
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(STDIN_FILENO, &fds);
                    int rv = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
                    if (rv > 0) { ev = TerminalEngine::ReadKey(); break; }
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - lastRenderTime).count();
                    if (elapsed >= refreshIntervalMs) {
                        timedOutNoInput = true;
                        break;
                    }
#endif
                }
            } else {
                ev = TerminalEngine::ReadKey();
            }

            // If the interval elapsed without user input, skip the key handling
            // and re-render the frame (continue outer loop) so live stats refresh.
            if (timedOutNoInput) continue;

            while (TerminalEngine::HasKeyPending()) {
                KeyEvent nextEv = TerminalEngine::ReadKey();
                if ((nextEv.key == Key::Up || nextEv.key == Key::Down) && nextEv.key == ev.key) {
                    if (ev.key == Key::Up) selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : static_cast<int>(options.size()) - 1;
                    else if (ev.key == Key::Down) selectedIndex = (selectedIndex + 1) % static_cast<int>(options.size());
                } else {
                    ev = nextEv;
                    break;
                }
            }

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
    static std::string ReadLine(const std::string& promptStr, const std::string& defaultVal = "", const std::vector<std::string>& suggestions = {}, const std::vector<std::string>& history = {}) {
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

                // 1. Try full suggestion prefix match
                for (const auto& sug : suggestions) {
                    std::string sugLower = sug;
                    std::transform(sugLower.begin(), sugLower.end(), sugLower.begin(), ::tolower);
                    if (sugLower.find(inputLower) == 0 && sug.length() > newInput.length()) {
                        ghost = sug.substr(newInput.length());
                        break;
                    }
                }

                // 2. Try last token prefix match against suggestion tokens
                if (ghost.empty()) {
                    size_t lastSpace = newInput.find_last_of(" '\"");
                    std::string lastToken = (lastSpace != std::string::npos) ? newInput.substr(lastSpace + 1) : newInput;
                    if (!lastToken.empty()) {
                        std::string tokenLower = lastToken;
                        std::transform(tokenLower.begin(), tokenLower.end(), tokenLower.begin(), ::tolower);
                        for (const auto& sug : suggestions) {
                            std::stringstream ss(sug);
                            std::string word;
                            while (ss >> word) {
                                std::string wLower = word;
                                std::transform(wLower.begin(), wLower.end(), wLower.begin(), ::tolower);
                                if (wLower.find(tokenLower) == 0 && word.length() > lastToken.length()) {
                                    ghost = word.substr(lastToken.length());
                                    break;
                                }
                            }
                            if (!ghost.empty()) break;
                        }
                    }
                }

                if (!ghost.empty()) {
                    std::cout << Color::Dim << ghost << Color::Reset;
                    for (size_t g = 0; g < ghost.length(); ++g) std::cout << "\b";
                }
            }
            std::cout << std::flush;
        };

        std::cout << Color::BrightCyan << Color::Bold << promptStr << Color::Reset << std::flush;
        const auto& activeHistory = !history.empty() ? history : suggestions;
        int historyIdx = static_cast<int>(activeHistory.size());

        while (true) {
            KeyEvent ev = TerminalEngine::ReadKey();
            if (ev.key == Key::Enter) {
                std::cout << "\n";
                if (input.empty()) return defaultVal;
                return input;
            } else if (ev.key == Key::Escape) {
                std::cout << "\n";
                return "";
            } else if (ev.key == Key::Tab || (ev.key == Key::Char && ev.ch == 9)) {
                if (!ghost.empty()) {
                    input += ghost;
                    updateDisplay(input);
                } else if (!input.empty() && !suggestions.empty()) {
                    std::string inputLower = input;
                    std::transform(inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower);
                    for (const auto& sug : suggestions) {
                        std::string sugLower = sug;
                        std::transform(sugLower.begin(), sugLower.end(), sugLower.begin(), ::tolower);
                        if (sugLower.find(inputLower) == 0 && sug.length() > input.length()) {
                            input = sug;
                            updateDisplay(input);
                            break;
                        }
                    }
                } else if (input.empty() && !suggestions.empty()) {
                    input = suggestions.front();
                    updateDisplay(input);
                }
            } else if (ev.key == Key::Up) {
                if (!activeHistory.empty() && historyIdx > 0) {
                    historyIdx--;
                    if (historyIdx >= 0 && historyIdx < static_cast<int>(activeHistory.size())) {
                        input = activeHistory[historyIdx];
                        updateDisplay(input);
                    }
                }
            } else if (ev.key == Key::Down) {
                if (!activeHistory.empty() && historyIdx < static_cast<int>(activeHistory.size()) - 1) {
                    historyIdx++;
                    input = activeHistory[historyIdx];
                    updateDisplay(input);
                } else if (!activeHistory.empty() && historyIdx == static_cast<int>(activeHistory.size()) - 1) {
                    historyIdx = static_cast<int>(activeHistory.size());
                    input = "";
                    updateDisplay(input);
                }
            } else if (ev.key == Key::Char) {
                if (ev.ch == 8 || ev.ch == 127) { // Backspace
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
