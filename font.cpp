#include "font.hpp"
#include <iterator>

FontCache font_cache;

FontCheckout FontCache::get(FontProperties props) {
    if (!cmap.count(props)) {

        std::string ttf_name = std::string(SDL_GetBasePath()) + "ttf/" + props.name;
        //ttf_name += props.bold ? (props.italic ? "-BoldItalic" : "-Bold") : (props.italic ? "-Italic" : "-Regular");
        ttf_name += ".ttf";
        TTF_Font *fon = TTF_OpenFont(ttf_name.c_str(),props.size);
        if (!fon) throw ttf_error("Failed to load font from "+ttf_name);
        TTF_SetFontHinting(fon,TTF_HINTING_MONO);

        cmap[props] = CacheLine {.font=fon};
    }

    return FontCheckout(&cmap[props],this);
}

Dimension FontCheckout::getGlyphDims() {
    // Only valid for monospace
    int h = TTF_FontLineSkip(get());
    int minx,maxx,miny,maxy,advance;
    TTF_GlyphMetrics(get(),'W',&minx,&maxx,&miny,&maxy,&advance);
    return {advance,h};
}


