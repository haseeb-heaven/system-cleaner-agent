#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <functional>
#include <iomanip>

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
    
    inline const std::string BgBlue      = "\033[44m";
    inline const std::string BgCyan      = "\033[46m";
    inline const std::string BgDarkGray  = "\033[100m";
}

enum class Key {
    Unknown,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Char
};

struct KeyEvent {
    Key key = Key::Unknown;
    char ch = 0;
};

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

    static void ClearScreen() {
        std::cout << "\033[2J\033[1;1H" << std::flush;
    }

    static void MoveCursor(int row, int col) {
        std::cout << "\033[" << row << ";" << col << "H" << std::flush;
    }

    static void HideCursor() {
        std::cout << "\033[?25l" << std::flush;
    }

    static void ShowCursor() {
        std::cout << "\033[?25h" << std::flush;
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

// Animated Spinner for Background Task Feedback
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

// Extended ASCII / Unicode Double-Line Box Renderer
class Box {
public:
    static std::string DrawBorder(int width, const std::string& title = "") {
        std::ostringstream ss;
        ss << Color::BrightCyan << "╔"; // Upper-left double box
        int titleLen = static_cast<int>(title.length());
        int lineLen = width - 2;
        if (titleLen > 0 && titleLen < lineLen - 4) {
            ss << "═══[ " << Color::BrightWhite << Color::Bold << title << Color::Reset << Color::BrightCyan << " ]";
            for (int i = 0; i < lineLen - titleLen - 7; ++i) ss << "═";
        } else {
            for (int i = 0; i < lineLen; ++i) ss << "═";
        }
        ss << "╗" << Color::Reset << "\n"; // Upper-right double box
        return ss.str();
    }

    static std::string DrawDivider(int width) {
        std::ostringstream ss;
        ss << Color::BrightCyan << "╠";
        for (int i = 0; i < width - 2; ++i) ss << "═";
        ss << "╣" << Color::Reset << "\n";
        return ss.str();
    }

    static std::string DrawFooter(int width) {
        std::ostringstream ss;
        ss << Color::BrightCyan << "╚";
        for (int i = 0; i < width - 2; ++i) ss << "═";
        ss << "╝" << Color::Reset << "\n";
        return ss.str();
    }

    static int GetVisibleDisplayWidth(const std::string& str) {
        int width = 0;
        size_t i = 0;
        while (i < str.length()) {
            if (str[i] == '\033') { // Skip ANSI escape sequences
                while (i < str.length() && str[i] != 'm') {
                    i++;
                }
                if (i < str.length()) i++; // skip 'm'
            } else {
                unsigned char c = static_cast<unsigned char>(str[i]);
                if (c < 0x80) {
                    width += 1;
                    i += 1;
                } else if ((c & 0xE0) == 0xC0) {
                    width += 1;
                    i += 2;
                } else if ((c & 0xF0) == 0xE0) {
                    width += 1; // 1 visual column for 3-byte UTF-8 (Braille ⠋, ⚪)
                    i += 3;
                } else if ((c & 0xF8) == 0xF0) {
                    width += 1;
                    i += 4;
                } else {
                    i += 1;
                }
            }
        }
        return width;
    }

    static std::string DrawLine(int width, const std::string& text, bool highlight = false) {
        std::ostringstream ss;
        ss << Color::BrightCyan << "║ " << Color::Reset;
        int visibleWidth = GetVisibleDisplayWidth(text);
        if (highlight) {
            ss << Color::BgBlue << Color::BrightWhite << Color::Bold << " ► " << text;
            int padding = width - visibleWidth - 7;
            int fill = (std::max)(0, padding);
            for (int i = 0; i < fill; ++i) ss << " ";
            ss << Color::Reset;
        } else {
            ss << "   " << text;
            int padding = width - visibleWidth - 7;
            int fill = (std::max)(0, padding);
            for (int i = 0; i < fill; ++i) ss << " ";
        }
        ss << Color::BrightCyan << " ║" << Color::Reset << "\n";
        return ss.str();
    }
};

// Professional Extended ASCII Block Loading Bar
class ProgressBar {
public:
    static std::string Render(double percentage, int width = 30) {
        percentage = (std::min)((std::max)(percentage, 0.0), 100.0);
        int filled = static_cast<int>((percentage / 100.0) * width);
        
        std::ostringstream ss;
        ss << Color::BrightCyan << "▕" << Color::BrightGreen;
        for (int i = 0; i < filled; ++i) ss << "█"; // Full block
        ss << Color::Dim;
        for (int i = filled; i < width; ++i) ss << "░"; // Light shade block
        ss << Color::Reset << Color::BrightCyan << "▏ " << Color::BrightWhite << Color::Bold
           << std::fixed << std::setprecision(1) << percentage << "%" << Color::Reset;
        return ss.str();
    }
};

class Menu {
    std::string title;
    std::vector<std::string> options;
    int selectedIndex = 0;
    std::string statusLine;
    SpinnerAnimation spinner;

public:
    Menu(const std::string& t, const std::vector<std::string>& opts)
        : title(t), options(opts) {}

    void SetStatusLine(const std::string& status) { statusLine = status; }
    int GetSelectedIndex() const { return selectedIndex; }

    int Show() {
        TerminalEngine::EnableVirtualTerminal();
        TerminalEngine::HideCursor();

        while (true) {
            TerminalEngine::ClearScreen();
            std::cout << Box::DrawBorder(80, title);

            for (size_t i = 0; i < options.size(); ++i) {
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                std::cout << Box::DrawLine(80, options[i], isSelected);
            }

            if (!statusLine.empty()) {
                std::cout << Box::DrawDivider(80);
                std::string animatedStatus = spinner.GetNextFrame() + " " + statusLine;
                std::cout << Box::DrawLine(80, animatedStatus, false);
            }

            std::cout << Box::DrawFooter(80);
            std::cout << "\033[90m Use UP/DOWN to navigate, Enter to select, ESC or 'q' to exit.\033[0m\n";

            KeyEvent ev = TerminalEngine::ReadKey();
            if (ev.key == Key::Up) {
                selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : static_cast<int>(options.size()) - 1;
            } else if (ev.key == Key::Down) {
                selectedIndex = (selectedIndex + 1) % static_cast<int>(options.size());
            } else if (ev.key == Key::Enter) {
                TerminalEngine::ShowCursor();
                return selectedIndex;
            } else if (ev.key == Key::Escape || (ev.key == Key::Char && (ev.ch == 'q' || ev.ch == 'Q'))) {
                TerminalEngine::ShowCursor();
                return -1;
            }
        }
    }
};

} // namespace OpenTUI
