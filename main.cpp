#include "main.hpp"
#include "terminal.hpp"
#include "font.hpp"
#include <cstdio>
#include <iostream>
#include <algorithm>
#include <queue>
#include <unordered_map>


static std::queue<std::string> input_queue;
static SDL_mutex *queue_mutex;
static SDL_Thread *input_thread;
static MainTerminalWindow *terminalWindow;
static SDL_mutex *terminal_mutex;
static std::unordered_map<std::string,std::unique_ptr<DebugWindow>> current_windows;


static int run_input_thread(void * _) {
    for(;;) {
        std::string line;
        for (;;) {
            char c;
            std::cin.get(c);
            if (c) {
                SDL_Lock lock (terminal_mutex); // Auto unlocks when it goes out of scope
                terminalWindow->putChar(c);
                //std::cout << int(c) << std::endl;
            }
            if (c==0 || c == '\n') break;
            else if (c == '\r') continue;
            else {
                line += c;
            }
        }
        if (!line.empty()) {
            SDL_Lock lock (queue_mutex); // Auto unlocks when it goes out of scope
            input_queue.push(std::move(line));
        }
    }
}

static bool trySetupWindow(const std::string type, std::string args) {

    std::unique_ptr<DebugWindow> *win = nullptr;

    // Check for a name 
    auto name_end = args.find(' ');
    if (name_end == std::string::npos) return false;
    std::string name = args.substr(0,name_end);
    args = args.substr(name_end+1);

    std::cout << "Trying to setup window of type \"" << type << "\" with name \"" << name << "\"?\n";

    std::string auto_title = name + " - " + type;

    if (current_windows.count(name)) {
        win = &current_windows[name];
    }

    if (type == "TERM") {
        if (!win || typeid(**win)!=typeid(DebugTerminalWindow)) {
            win = &(current_windows[name]=std::make_unique<DebugTerminalWindow>(std::move(auto_title)));
        }
    }

    if (win) {
        std::cout << "parsing setup\n";
        (*win)->parse_setup(args);
        return true;
    } else return false;

    
}


int main(int argc, char* argv[]) {

    std::cerr << "SDL2 P2 debugger...\n";
    if (SDL_Init(SDL_INIT_VIDEO)) throw sdl_error("SDL2 init error");

    if (TTF_Init()) throw ttf_error("SDL2_TTF init error");

    // Terminal window is handled directly by the input thread, so don't touch it too much
    std::cout << "aaaaaaa\n";
    terminalWindow = new MainTerminalWindow();
    std::cout << "bbbbbbbb\n";


    // Force standard input to be unbuffered (doesn't work for terminal????)
    std::cin.rdbuf()->pubsetbuf(nullptr,0);

    queue_mutex = SDL_CreateMutex();
    if (!queue_mutex) throw sdl_error("Failed to create queue lock");

    terminal_mutex = SDL_CreateMutex();
    if (!terminal_mutex) throw sdl_error("Failed to create terminal lock");
    
    input_thread = SDL_CreateThread(run_input_thread,"Data Input",NULL);
    if (!input_thread) throw sdl_error("Failed to create input thread");

    std::cout << "init ok\n";

    for (;;) {
        SDL_Event ev;
        if (SDL_PollEvent(&ev)) {
            switch(ev.type) {
                case SDL_QUIT:
                    std::cout << "Got quit event\n";
                    goto quit;
                case SDL_WINDOWEVENT: {
                    AppWindow *affected_win;
                    const std::string *affected_name = nullptr;
                    if (terminalWindow->idMatchesWindow(ev.window.windowID)) {
                        affected_win = terminalWindow;
                    } else {
                        auto find_iter = std::find_if(current_windows.begin(),current_windows.end(),[ev](auto &pair){return pair.second->idMatchesWindow(ev.window.windowID);});
                        if (find_iter == current_windows.end()) {
                            std::cout << "Spurious window event?????\n"; // WTF??
                            break; 
                        }
                        affected_win = find_iter->second.get();
                        affected_name = &find_iter->first;
                    }
                    std::cout << "got win event " << int(ev.window.event) << " on " << (affected_name ? *affected_name : "Main window"s) << std::endl;
                    switch (ev.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        if (affected_name) {
                            // Destroy the window
                            current_windows.erase(*affected_name);
                        } else {
                            // If closing main terminal, just quit
                            SDL_Event quitEv = {.type=SDL_QUIT};
                            SDL_PushEvent(&quitEv);
                        }
                        break;
                    default:
                        affected_win->handleWindowEvent(ev);
                        break;
                    }
                }
                default: break;
            }
        }

        std::queue<std::string> dispatch_queue;
        {
            SDL_Lock lock (queue_mutex); // Auto unlocks when it goes out of scope
            while (!input_queue.empty()) {
                std::string &line = input_queue.front();
                if (line[0] == '`') dispatch_queue.push(std::move(line)); // Dispatch this later (when the input is unlocked again)
                //std::cout << input_queue.front() << std::endl;
                input_queue.pop();
            }
        }
        while (!dispatch_queue.empty()) {
            std::string line = std::move(dispatch_queue.front());
            dispatch_queue.pop();

            auto ident_end = line.find(' ',1);
            if (ident_end == std::string::npos) continue;
            std::string ident = line.substr(1,ident_end-1);
            if (current_windows.count(ident)) {
                // Dispatch to window
                current_windows[ident]->parse_data(line.substr(ident_end+1));
            } else if (trySetupWindow(ident,line.substr(ident_end+1))) {
                
            }


        }

        // Update dirty windows
        for (const auto & [name, win]  : current_windows) {
            if (win->isDirty()) {
                win->repaint();
            }
        }

        // Update main terminal
        if (terminalWindow->isDirty()) {
            SDL_Lock lock (terminal_mutex); // Auto unlocks when it goes out of scope
            terminalWindow->repaint();
        }

        SDL_Delay(16);
    }

    quit:

    delete terminalWindow;
    terminalWindow = nullptr;

    SDL_Quit();
    return 0;
}


AppWindow::AppWindow() {
    // Nothing to do here for now...
}

void AppWindow::repaint() {
    // Just make sure the window is set up
    const char *title = get_title();
    if (!handle) {
        std::cout << "Window size: " << dim.width << ", " << dim.height << std::endl;
        handle = SDL_CreateWindow(title,SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,dim.width,dim.height,getWindowFlags());
        if (!handle) throw sdl_error("Failed to create window \""s + title + "\"");
    }
    if (!strcmp(SDL_GetWindowTitle(handle),title)) SDL_SetWindowTitle(handle,title);
    int w,h;
    SDL_GetWindowSize(handle,&w,&h);
    if (w != dim.width || h != dim.height) SDL_SetWindowSize(handle,dim.width,dim.height);

    dirty = false;
}

AppWindow::~AppWindow() {
    if (handle) SDL_DestroyWindow(handle);
}

token_iterator token_iterator::begin(const std::string &str) {
    token_iterator i = {std::string_view(str.c_str(),0)};
    return ++i;
}
token_iterator token_iterator::end(const std::string &str) {
    return {std::string_view(str.c_str()+str.size(),0)};
}

token_iterator& token_iterator::operator++() {

    const char *start = view.data()+view.size();
    // Advance past spaces
    while (*start == ' ') start++;

    if (*start == 0) {
        view = std::string_view(start,0);
        return *this;
    }
    const char *end = start;
    bool is_string = *start == '\'';
    while (*end != 0 && (start == end || *end != (is_string?'\'':' '))) end++;
    if (is_string) end++;

    view = std::string_view(start,end-start);

    return *this;
};

token_iterator::token_kind token_iterator::classify() const {
    char c = view.front();
    if (view.size() == 0) {
        switch (c) {
        case 0: return TOKEN_END;
        default: return TOKEN_ERROR;
        }
    } else {
        switch (c) {
        case 0: return TOKEN_ERROR;
        case '\'': return TOKEN_STRING;
        case 'a' ... 'z': case 'A' ... 'Z': return TOKEN_SYMBOL;
        case '0' ... '9': case '-': case '+': return TOKEN_NUMBER;
        default: return TOKEN_ERROR;
        }
    }
}

token_iterator::value_type token_iterator::get_symbol(const std::string& desc) {
    expect(TOKEN_SYMBOL,desc);
    value_type sview = view;
    (*this)++;
    return sview;
}

token_iterator::value_type token_iterator::get_string(const std::string& desc) {
    expect(TOKEN_STRING,desc);
    value_type sview = view;
    sview.remove_prefix(1); // Get rid of quotes
    sview.remove_suffix(1);
    (*this)++;
    return sview;
}

// This is slightly painful
int token_iterator::get_int(const std::string& desc) {
    expect(TOKEN_NUMBER,desc);
    value_type pview = view;
    bool negative = false;
    int val = 0;
    switch(pview.front()) {
    case '-':
        negative = true;
        // fall through
    case '+':
        pview.remove_prefix(1);
        break;
    }
    for (char c : pview) {
        if (c=='_') continue;
        if (c < '0' || c > '9') throw token_error("Bad char "+std::to_string(int(c))+"in integer!");
        val = val*10 + c - '0';
    }
    (*this)++;
    return negative ? -val : +val;
}


bool token_iterator::is_color(const std::string& desc) const {
    switch (classify()) {
    case TOKEN_NUMBER:  return true; // Presume this is an RGB color
    case TOKEN_SYMBOL: {
        auto symbol = **this;
        if (casecompare(symbol,"WHITE")) return true;
        else if (casecompare(symbol,"BLACK")) return true;
        else if (casecompare(symbol,"GREY")) return true;
        else if (casecompare(symbol,"YELLOW")) return true;
        else if (casecompare(symbol,"MAGENTA")) return true;
        else if (casecompare(symbol,"RED")) return true;
        else if (casecompare(symbol,"CYAN")) return true;
        else if (casecompare(symbol,"GREEN")) return true;
        else if (casecompare(symbol,"BLUE")) return true;
        else if (casecompare(symbol,"ORANGE")) return true;
        else return false;
    }
    default: return false;
    }
}


SDL_Color token_iterator::get_color(const std::string& desc) {
    switch (classify()) {
    case TOKEN_NUMBER: {// Presume this is an RGB color
        int c = get_int();
        return {.r=c&255,.g=(c>>8)&255,.b=(c>>16)&255};
    } break;
    case TOKEN_SYMBOL: {
        auto symbol = get_symbol();
        SDL_Color basecol;
        bool need_intensity = true;
        if (casecompare(symbol,"WHITE")) {
            basecol = {255,255,255};
            need_intensity = false;
        } else if (casecompare(symbol,"BLACK")) {
            basecol = {0,0,0};
            need_intensity = false;
        } 
        else if (casecompare(symbol,"GREY")) basecol = {255,255,255};
        else if (casecompare(symbol,"YELLOW")) basecol = {255,255,0};
        else if (casecompare(symbol,"MAGENTA")) basecol = {255,0,255};
        else if (casecompare(symbol,"RED")) basecol = {255,0,0};
        else if (casecompare(symbol,"CYAN")) basecol = {0,255,255};
        else if (casecompare(symbol,"GREEN")) basecol = {0,255,0};
        else if (casecompare(symbol,"BLUE")) basecol = {0,0,255};
        else if (casecompare(symbol,"ORANGE")) basecol = {255,127,0};
        else {
            throw token_error("Unknown color \""s+std::string(symbol)+"\"");
        }
        if (need_intensity) {
            short i = classify() == TOKEN_NUMBER ? std::clamp(get_int(),0,15) : 8;
            short lighten = (i-8)<<5;
            if (lighten>0) return {std::min(255,basecol.r+lighten),std::min(255,basecol.g+lighten),std::min(255,basecol.b+lighten)};
            else return {(basecol.r*i)>>3,(basecol.g*i)>>3,(basecol.b*i)>>3};
        } else return basecol;
    }
    default:
        throw token_error("While "+desc+": Expected NUMBER or SYMBOL");
    }
}

bool DebugWindow::try_parse_common_setup_sym(std::string_view symbol, token_iterator &iter) {
    if (casecompare(symbol,"POS")) {
        int x = iter.get_int("Getting POS X");
        int y = iter.get_int("Getting POS Y");
        // TODO: actually handle this
    } else if (casecompare(symbol,"TITLE")) {
        title = iter.get_string("Getting TITLE");
        dirty = true;
    } else {
        return false;
    }

    return true;
}

