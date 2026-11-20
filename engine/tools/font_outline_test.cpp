#include "gfx/font_outline.hpp"

#include <cstdint>
#include <iostream>

namespace {

uint8_t alphaAt(const cr::Image& image, int x, int y) {
    return image.rgba[(size_t(y) * size_t(image.width) + size_t(x)) * 4 + 3];
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    cr::Image glyph;
    glyph.width = glyph.height = 7;
    glyph.rgba.assign(7u * 7u * 4u, 0);
    const size_t solid = (3u * 7u + 3u) * 4u;
    glyph.rgba[solid] = glyph.rgba[solid + 1] = glyph.rgba[solid + 2] = 255;
    glyph.rgba[solid + 3] = 255;
    const size_t antialias = (3u * 7u + 4u) * 4u;
    glyph.rgba[antialias] = glyph.rgba[antialias + 1] = glyph.rgba[antialias + 2] = 255;
    glyph.rgba[antialias + 3] = 128;
    cr::Image edge;
    if (!cr::makeGlyphOutline(glyph, 2, edge)) return fail("outline generation failed");
    if (edge.width != glyph.width || edge.height != glyph.height) return fail("outline dimensions changed");
    if (alphaAt(edge, 3, 3) != 0) return fail("outline filled the glyph interior");
    if (alphaAt(edge, 4, 3) != 127) return fail("antialiased edge coverage was not preserved");
    if (alphaAt(edge, 5, 3) != 255) return fail("outline did not reach the requested radius");
    if (alphaAt(edge, 5, 5) != 0) return fail("outline exceeded its circular radius");
    if (alphaAt(edge, 6, 3) != 128) return fail("antialiased coverage did not reach the outline radius");
    if (alphaAt(edge, 6, 5) != 0) return fail("outline extended beyond its circular radius");
    return 0;
}
