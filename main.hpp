#pragma once
#include <cstdint>
#include <climits>
#include <stdexcept>
#include <memory>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

using uint = unsigned;

using namespace std::string_literals;

struct Dimension {
    int width,height;
};

inline bool casecompare(const std::string_view &a, const std::string_view &b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(),a.end(),b.begin(),[](char x, char y){return x==y || std::toupper(x) == std::toupper(y);});
}

inline bool same_view(const std::string_view &a, const std::string_view &b) {
    return a.data() == b.data() && a.size() == b.size();
}

class token_error : public std::runtime_error {
    public:
        token_error(const std::string& desc) : std::runtime_error(desc) {};
};

class token_iterator {
    public:

        enum token_kind {
            TOKEN_END,
            TOKEN_ERROR,
            TOKEN_SYMBOL,
            TOKEN_STRING,
            TOKEN_NUMBER,
        };
        
        static constexpr const char* token_kind_names[] = {
            "END",
            "ERROR",
            "SYMBOL",
            "STRING",
            "NUMBER",
        };

        class token_expected : public token_error {
            public:
                token_expected(token_iterator iter,token_kind expect, const std::string& desc = "")
                : token_error((desc.empty()?desc:desc+". ")+"Expected "+token_kind_names[expect]+", got "+token_kind_names[iter.classify()]+(iter.classify()>TOKEN_ERROR?" ("+std::string(*iter)+")":""))
                {};
        };

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::string_view;
        using pointer = const value_type *;
        using reference = const value_type &;

        reference operator*() const {return view;};
        pointer operator->() {return &view;};

        token_iterator& operator++();
        token_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }

        static token_iterator begin(const std::string &str);
        static token_iterator end(const std::string &str);

        token_kind classify() const;
        void expect(token_kind kind, const std::string& desc = "") const {
            token_kind got = classify();
            if (got!=kind) throw token_expected(*this,kind,desc);
        };

        value_type get_string(const std::string& desc = "");
        int get_int(const std::string& desc = "");
        value_type get_symbol(const std::string& desc = "");

        bool is_color(const std::string& desc = "") const;
        SDL_Color get_color(const std::string& desc = "");

        friend bool operator== (const token_iterator &a, const token_iterator &b) {return same_view(a.view,b.view);};
        friend bool operator!= (const token_iterator &a, const token_iterator &b) {return !same_view(a.view,b.view);};

    private:
        std::string_view view;
        token_iterator(std::string_view v) : view{v} {};
};



class AppWindow {
    public:
        Dimension dim = {0,0};
        virtual const char *get_title() = 0;
        bool idMatchesWindow(uint32_t winID) {
            return SDL_GetWindowID(handle) == winID;
        };


        AppWindow();
        virtual ~AppWindow();
        virtual void repaint();
        virtual bool handleWindowEvent(SDL_Event &ev) {return false;};

        bool isDirty() {return dirty;};

    protected:
        SDL_Window *handle = nullptr;
        bool dirty = true;
        
        virtual uint32_t getWindowFlags() {
            return 0;
        }
};

class DebugWindow : public virtual AppWindow {
    public:
        virtual void parse_setup(const std::string &str) = 0;
        virtual void parse_data(const std::string &str) = 0;
        virtual const char *get_title() {return title.c_str();};
        DebugWindow(std::string title) : AppWindow(), title{title} {};
    protected:
        std::string title;
        
        bool try_parse_common_setup_sym(std::string_view symbol, token_iterator &iter);
};

// For C++ RAII magic
class SDL_Lock {
    private:
        SDL_mutex *m;
    public:
        SDL_Lock(SDL_mutex *mutex) : m{mutex} {
            SDL_LockMutex(m);
        };
        ~SDL_Lock() {
            SDL_UnlockMutex(m);
        };
        SDL_Lock(const SDL_Lock &) = delete;
        SDL_Lock &operator=(const SDL_Lock &) = delete;
};

class sdl_error : public std::runtime_error {
    public:
        sdl_error(const std::string& desc, const char *err = SDL_GetError()) : std::runtime_error(desc + " (" + err + ")") {};
};

class ttf_error : public std::runtime_error {
    public:
        ttf_error(const std::string& desc, const char *err = TTF_GetError()) : std::runtime_error(desc + " (" + err + ")") {};
};


