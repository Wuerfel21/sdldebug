#include "terminal.hpp"
#include <algorithm>
#include <iostream>

TerminalWindow::TerminalWindow() : grid{nullptr}, global_bg{0,0,0},current_fg{0,255,0},current_bg{0,0,64} {
    resize({.cols=40,.rows=20});
}

MainTerminalWindow::MainTerminalWindow() {
    resize({.cols=40,.rows=25});
}

DebugTerminalWindow::DebugTerminalWindow(std::string title) : DebugWindow(title),TerminalWindow(),term_colors{default_colors} {
    selectColors(0);
}


void TerminalWindow::resize(TerminalDimension newdim) {
    auto oldDim = termDim;
    termDim = newdim;

    auto oldGrid = grid;

    grid = new termchar_t[termDim.cols*termDim.rows];

    for (int y=0;y<termDim.rows;y++) {
        for (int x=0;x<termDim.cols;x++) {
            termchar_t *c = &grid[y*termDim.cols+x];
            if (oldGrid && x<oldDim.cols && y<oldDim.rows) *c = oldGrid[y*oldDim.cols+x];
            else *c = {.ch=' ',.fg=current_fg,.bg=current_bg};
        }
    }

    if (oldGrid) delete oldGrid;

    cursorX = std::clamp(cursorX,0,termDim.cols-1);
    cursorY = std::clamp(cursorX,0,termDim.rows-1);
    allDirty();
}

void TerminalWindow::putChar(wchar_t c) {
    if (typeid(this) == typeid(DebugTerminalWindow)) std::cout << "got char " << int(c) << std::endl;
    switch (c) {
    case 0: // clear+home
        for(int y=0;y<termDim.rows;y++)for(int x=0;x<termDim.cols;x++) setCharAt(x,y,{.ch=' ',.fg=current_fg,.bg=global_bg});
        allDirty();
        // Fall through
    case 1: // home
        cursorX = cursorY = 0;
        break;
    case 2: // set cursor X
        // TODO
        break;
    case 3: // set cursor Y
        // TODO
        break;
    case 4 ... 7: // set color set
        selectColors(c-4);
        break;
    case 8: // Backspace
        if (--cursorX < 0) {
            cursorX = 0;
            if(cursorY>0)--cursorY;
        }
        break;
    case 9: // Tab
        cursorX = std::clamp((cursorX+8)&~7,0,termDim.cols-1);
        break;
    case 10: // == \r
        newLine();
        lastNewLine = c;
        return;
    case 11 ... 12: break; // Unused?
    case 13: // == \n
        if(lastNewLine!=10) newLine();
        lastNewLine = c;
        return;
    case 14 ... 31: break; // Unused?
    default:
        if (cursorX >= termDim.cols) newLine();
        setCharAt(cursorX++,cursorY,c);
        break;
    }

}

void TerminalWindow::newLine() {
    if (cursorY >= termDim.rows-1) {
        memmove(&grid[0],&grid[termDim.cols],termDim.cols*(termDim.rows-1)*sizeof(termchar_t));
        for (int i=0;i<termDim.cols;i++) setCharAt(i,termDim.rows-1,' ');
        allDirty();
    } else {
        cursorY++;
    }
    cursorX = 0;
}


void TerminalWindow::repaint() {


    Dimension glyphDims = fnt.getGlyphDims();
    //std::cout << "gylph dims are " << glyphDims.width << " and " << glyphDims.height << std::endl;  
    dim = {.width=glyphDims.width*termDim.cols,.height=glyphDims.height*termDim.rows};

    AppWindow::repaint(); // Make sure window is ready

    int repaintXMin = std::clamp(dirtyXMin,0,termDim.cols-1);
    int repaintYMin = std::clamp(dirtyYMin,0,termDim.rows-1);
    int repaintXMax = std::clamp(dirtyXMax,0,termDim.cols-1);
    int repaintYMax = std::clamp(dirtyYMax,0,termDim.rows-1);

    bool needSurfaceRepaint = !(repaintXMin > repaintXMax || repaintYMin > repaintYMax);
    //std::cout << "dirty area is {[" << dirtyXMin << "," << dirtyXMax << "],[" << dirtyYMin << "," << dirtyYMax << "]}" << std::endl;
    //std::cout << "repaint area is {[" << repaintXMin << "," << repaintXMax << "],[" << repaintYMin << "," << repaintYMax << "]}" << std::endl;

    if (needSurfaceRepaint) {

        SDL_Surface *win_surf = SDL_GetWindowSurface(handle);

        

        // TODO: arguably this is very terrible, needs glyph cache
        for (int y=repaintYMin;y<=repaintYMax;y++) {
            for (int x=repaintXMin;x<=repaintXMax;x++) {
                termchar_t chr = getCharAt(x,y);
                SDL_Surface *glyph = TTF_RenderGlyph_Shaded(fnt.get(),chr.ch,chr.fg,chr.bg);
                SDL_Rect target = {.x=x*glyphDims.width,.y=y*glyphDims.height,.w=glyphDims.width,.h=glyphDims.height};
                if (glyph->w < glyphDims.width || glyph->h < glyphDims.height) {
                    if(SDL_FillRect(win_surf,&target,SDL_MapRGB(win_surf->format,chr.bg.r,chr.bg.g,chr.bg.b))) {
                        throw sdl_error("glyph BG fill failed");
                    }
                }
                if(SDL_BlitSurface(glyph,NULL,win_surf,&target)) throw sdl_error("glyph blit failed");
                SDL_FreeSurface(glyph);
            }
        }

    }
    
    if (needSurfaceRepaint) {
        SDL_Rect target = {.x=repaintXMin*glyphDims.width,.y=repaintYMin*glyphDims.height,.w=(repaintXMax+1)*glyphDims.width,.h=(repaintYMax+1)*glyphDims.height};
        SDL_UpdateWindowSurfaceRects(handle,&target,1);
    } else {
        SDL_UpdateWindowSurface(handle);
    }
    allClean();
}

void DebugTerminalWindow::parse_setup(const std::string &str) {
    auto iter = token_iterator::begin(str);
    auto end = token_iterator::end(str);
    while (iter!=end) {
        /*
        std::cout << "Got token of size " << iter->length() << " and kind " << iter.classify() << " and offset " << int(iter->data() - str.c_str()) <<std::endl;
        std::cout << "first char is '" <<  iter->front() << "'\n";
        if (iter->length()) std::cout << "contents are: " << *iter << std::endl;
        SDL_Delay(200);
        */
        
        auto symbol = iter.get_symbol("Getting next setup symbol");
        std::cout << "trying to parse setup symbol "<<symbol<<std::endl;
        if (try_parse_common_setup_sym(symbol,iter)) {
            // We good.
        } else if (casecompare(symbol,"SIZE")) {
            int cols = iter.get_int("Getting column count");
            int rows = iter.get_int("Getting row count");
            std::cout << "resizing to " << cols << ", " << rows <<std::endl;
            resize({.cols=cols,.rows=rows});
            std::cout << "token after getting size: " << *iter << std::endl;
        } else if (casecompare(symbol,"TEXTSIZE")) {
            int size = iter.get_int("Getting TEXTSIZE");
            using_font.size = size;
            fnt = loadFont();
            allDirty();
        } else if (casecompare(symbol,"COLOR")) {       
            for(int i = 0;;i++) {
                std::cout << "alleged color is "<<*iter<<std::endl;
                bool is_color = iter.is_color();
                if (i==0 && !is_color) throw token_error("expected at least one color");
                if (i>=8 && is_color) throw token_error("too many colors");
                if (!is_color) break;
                term_colors[i] = iter.get_color();
            }
            selectColors(last_selected_colors);
        } else {
            std::cerr << "Unhandled symbol " << symbol << std::endl;
            while (iter != end && iter.classify() != token_iterator::TOKEN_SYMBOL) ++iter;
        }
    }
}

void DebugTerminalWindow::parse_data(const std::string &str) {
    auto iter = token_iterator::begin(str);
    auto end = token_iterator::end(str);
    while(iter!=end) {
        std::cout << "got data token "<<*iter<<std::endl;
        switch(iter.classify()) {
        case token_iterator::TOKEN_NUMBER:
            putChar(iter.get_int());
            break;
        case token_iterator::TOKEN_STRING:
            for (char c : iter.get_string()) {
                putChar(c);
            }
            break;
        case token_iterator::TOKEN_SYMBOL: {
            auto symbol = iter.get_symbol();
            if (try_parse_common_data_sym(symbol,iter)) {
                // ok
            } else {
                throw token_error("Unhandled symbol \""s+std::string(symbol)+"\" in terminal data");
            }
        } break;
        default:
            throw token_error("Erroneous terminal data token");
            ++iter;
        }
    }
}


bool MainTerminalWindow::handleWindowEvent(SDL_Event &ev) {
    if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        std::cout << "Window resized to " << ev.window.data1 << " x " << ev.window.data2 << std::endl;
        Dimension glyphDims = fnt.getGlyphDims();
        TerminalDimension newSize = {.cols=ev.window.data1/glyphDims.width,.rows=ev.window.data2/glyphDims.height};
        if (newSize.cols > 0 && newSize.rows > 0 && newSize != termDim) resize(newSize);
        return true;
    } else return false;
};
