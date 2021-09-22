#pragma once
#include "main.hpp"
#include "font.hpp"
#include <algorithm>

struct TerminalDimension {
    int cols,rows;
    Dimension computePixelSize(TTF_Font *font);
    friend bool operator==(const TerminalDimension &a,const TerminalDimension &b) {
        return a.cols == b.cols && a.rows == b.rows;
    }
    friend bool operator!=(const TerminalDimension &a,const TerminalDimension &b) {return !(a==b);};
};

struct termchar_t {
    wchar_t ch;
    SDL_Color fg,bg;
};

class TerminalWindow : public virtual AppWindow {
    protected:
        TerminalDimension termDim;
        termchar_t *grid;
        FontCheckout fnt = loadFont();
        virtual FontCheckout loadFont() {return font_cache.get({.name=default_typeface,.size=16});};
        int dirtyXMin, dirtyYMin, dirtyXMax, dirtyYMax;
        void allDirty() {dirty = true; dirtyXMin = INT_MIN, dirtyYMin = INT_MIN, dirtyXMax = INT_MAX, dirtyYMax = INT_MAX;};
        void allClean() {dirty = false; dirtyXMin = INT_MAX, dirtyYMin = INT_MAX, dirtyXMax = INT_MIN, dirtyYMax = INT_MIN;};
        void newLine();

        // Terminal state
        int cursorX=0,cursorY=0;
        wchar_t lastNewLine=0;
        wchar_t lastSpecial=0;

    public:
        SDL_Color global_bg, current_fg, current_bg;
        virtual uint32_t getWindowFlags() {
            return 0;
        };
        virtual void repaint();
        termchar_t getCharAt(int x,int y) {
            if (x>=termDim.cols||y>=termDim.rows) throw std::out_of_range("Coordinates out of range");
            if (!grid) return {.ch='!',.fg=current_fg,.bg=current_bg};
            return grid[x+y*termDim.cols];
        }
        void setCharAt(int x,int y,termchar_t c) {
            if (x>=termDim.cols||y>=termDim.rows) throw std::out_of_range("Coordinates out of range");
            if (!grid) throw std::runtime_error("grid is null");
            grid[x+y*termDim.cols] = c;
            dirty = true;
            dirtyXMin = std::min(dirtyXMin,x);
            dirtyXMax = std::max(dirtyXMax,x);
            dirtyYMin = std::min(dirtyYMin,y);
            dirtyYMax = std::max(dirtyYMax,y);
        }
        void setCharAt(int x,int y,wchar_t c) {
            setCharAt(x,y,{.ch=c,.fg=current_fg,.bg=current_bg});
        }
        void clear();
        void putChar(wchar_t c);
        void resize(TerminalDimension termDim);
        virtual void selectColors(int i) = 0;
        TerminalWindow();

};

class MainTerminalWindow : public TerminalWindow {
    protected:
    public:
        virtual void selectColors(int i) {
            // No-Op
        };
        virtual const char *get_title() {return "DEBUG Output";};
        MainTerminalWindow();
        virtual uint32_t getWindowFlags() {return TerminalWindow::getWindowFlags()|SDL_WINDOW_RESIZABLE;};
        virtual bool handleWindowEvent(SDL_Event &ev);
};

class DebugTerminalWindow : public DebugWindow, TerminalWindow {
    protected:
        FontProperties using_font = {.name=default_typeface,.size=16};
        virtual FontCheckout loadFont() {return font_cache.get(using_font);};
        virtual void parse_setup(const std::string &str);
        virtual void parse_data(const std::string &str);
        uint8_t last_selected_colors;
    public:
        virtual void selectColors(int i) {
            i = std::clamp(i,0,4);
            last_selected_colors = i;
            current_fg = term_colors[i*2];
            current_bg = term_colors[i*2+1];
        };
        static const constexpr std::array<SDL_Color,8> default_colors = {{
            {255,127,0},{0,0,0},
            {0,0,0},{255,127,0},
            {0,255,0},{0,0,0},
            {0,0,0},{0,255,0},
        }};
        std::array<SDL_Color,8> term_colors;
        DebugTerminalWindow(std::string title);
};
