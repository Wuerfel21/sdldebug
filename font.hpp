#pragma once
#include "main.hpp"
#include <unordered_map>

const std::string default_typeface = "Parallax";

struct FontProperties {
    const std::string name;
    int size;
    bool bold:1;
    bool italic:1;

    bool operator==(const FontProperties &that) const {
        return this->name == that.name && this->size == that.size && this->bold == that.bold && this->italic == that.italic;
    };
};

namespace std {
    template<>
    struct hash<FontProperties> {
        inline size_t operator()(const FontProperties& props) const {
            return std::hash<std::string>{}(props.name)+props.size+props.bold+props.italic;
        }
    };
}

class FontCheckout;


class FontCache {
    friend FontCheckout;
    private:
        struct CacheLine {
            TTF_Font *font;
            uint users;
        };
        std::unordered_map<FontProperties,CacheLine> cmap;

    public:  
        FontCheckout get(FontProperties props);
};


class FontCheckout {
    friend FontCache;
    private:
        FontCache::CacheLine *fon;
        FontCache *cache;
        FontCheckout(FontCache::CacheLine *fon,FontCache *cache) : fon{fon},cache{cache} {
            fon->users++;
        };
        FontCheckout(const FontCache &) {
            fon->users++;
        };
    public:
        ~FontCheckout() {
            --fon->users;
        }
        TTF_Font *get() {return fon->font;};
        Dimension getGlyphDims();

};

extern FontCache font_cache;

