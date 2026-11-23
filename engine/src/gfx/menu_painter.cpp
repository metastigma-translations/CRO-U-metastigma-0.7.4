#include "gfx/menu_painter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/text.hpp"
#include "game/stage_tables.hpp"
#include "gfx/font.hpp"
#include "gfx/font_outline.hpp"

namespace cr {

namespace {
constexpr float kScale = 2.0f; // menu units -> 1024x768 screen
} // namespace

void MenuPainter::release(Renderer& r) {
    for (auto& [s, c] : text_) {
        r.destroy(c.tex);
        r.destroy(c.edgeTex);
    }
    text_.clear();
    for (auto& [key, tex] : gradients_) r.destroy(tex);
    gradients_.clear();
}

void MenuPainter::piece(float x, float y, float u, float v, float w, float h, float alpha, float r, float g, float b) {
    if (!r_ || !sheet_ || !sheet_->id || alpha <= 0.001f) return;
    r_->drawQuadUV(*sheet_, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale, u * kScale, v * kScale,
                   w * kScale, h * kScale, r, g, b, alpha);
}

void MenuPainter::pieceTo(float x, float y, float w, float h, float u, float v, float uw, float vh, float alpha, float r,
                          float g, float b) {
    if (!r_ || !sheet_ || !sheet_->id || alpha <= 0.001f) return;
    r_->drawQuadUV(*sheet_, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale, u * kScale, v * kScale,
                   uw * kScale, vh * kScale, r, g, b, alpha);
}

void MenuPainter::image(const Texture& tex, float x, float y, float w, float h, float alpha) {
    if (!r_ || !tex.id || alpha <= 0.001f) return;
    r_->drawQuad(tex, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale, 1, 1, 1, alpha);
}

void MenuPainter::imagePiece(const Texture& tex, float x, float y, float u, float v, float w, float h, float alpha) {
    if (!r_ || !tex.id || alpha <= 0.001f) return;
    r_->drawQuadUV(tex, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale, u, v, w, h, 1, 1, 1, alpha);
}

void MenuPainter::imagePieceTo(const Texture& tex, float x, float y, float w, float h, float u, float v, float uw, float vh,
                               float alpha, float r, float g, float b) {
    if (!r_ || !tex.id || alpha <= 0.001f) return;
    r_->drawQuadUV(tex, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale, u, v, uw, vh, r, g, b, alpha);
}

void MenuPainter::picture(int pict, float x, float y, float alpha) {
    if (pict < 0 || pict >= kNumMenuPictures) return;
    const short* p = kMenuPicture[pict];
    piece(x, y, p[0], p[1], p[2], p[3], alpha);
}

void MenuPainter::stretch(int pict, float x, float y, float w, float h, float alpha) {
    if (pict < 0 || pict >= kNumMenuPictures || !r_ || !sheet_ || !sheet_->id || alpha <= 0.001f) return;
    const short* p = kMenuPicture[pict];
    r_->drawQuadUV(*sheet_, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale, p[0] * kScale, p[1] * kScale,
                   p[2] * kScale, p[3] * kScale, 1, 1, 1, alpha);
}

void MenuPainter::quad(float x, float y, float w, float h, const float* left, const float* right) {
    if (!r_ || w <= 0 || h <= 0) return;
    const float opacity = std::clamp(std::max(left[3], right[3]), 0.0f, 1.0f);
    if (opacity <= 0.001f) return;

    const auto byte = [](float v) {
        return uint8_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    const float leftAlpha = std::clamp(left[3] / opacity, 0.0f, 1.0f);
    const float rightAlpha = std::clamp(right[3] / opacity, 0.0f, 1.0f);
    const std::array<uint8_t, 8> key = {
        byte(left[0]), byte(left[1]), byte(left[2]),
        byte(right[0]), byte(right[1]), byte(right[2]),
        byte(leftAlpha), byte(rightAlpha)
    };

    auto it = gradients_.find(key);
    if (it == gradients_.end()) {
        // A single 1024-sample texture is linearly filtered by both UI backends.
        // This removes the alpha hairlines and hard seams from adjacent flat quads.
        constexpr int kGradientWidth = 1024;
        Image img;
        img.width = kGradientWidth;
        img.height = 1;
        img.rgba.resize(size_t(kGradientWidth) * 4);
        for (int i = 0; i < kGradientWidth; ++i) {
            const float t = float(i) / float(kGradientWidth - 1);
            uint8_t* pixel = img.rgba.data() + size_t(i) * 4;
            for (int c = 0; c < 3; ++c)
                pixel[c] = byte(float(key[size_t(c)]) / 255.0f * (1 - t) +
                                float(key[size_t(c + 3)]) / 255.0f * t);
            pixel[3] = byte(float(key[6]) / 255.0f * (1 - t) + float(key[7]) / 255.0f * t);
        }
        Texture tex = r_->upload(img);
        if (!tex.id) return;
        it = gradients_.emplace(key, tex).first;
    }
    r_->drawQuad(it->second, X(x) * kScale, Y(y) * kScale, w * K() * kScale, h * K() * kScale,
                 1, 1, 1, opacity);
}

void MenuPainter::slant(float x, float y, float w, float h, float lean, const float* left, const float* right) {
    if (!r_) return;
    const int strips = left[3] == right[3] && left[0] == right[0] && left[1] == right[1] && left[2] == right[2]
                           ? 1 : std::max(1, int(w * kScale / 4));
    for (int i = 0; i < strips; ++i) {
        const float t = (i + 0.5f) / strips;
        float c[4];
        for (int k = 0; k < 4; ++k) c[k] = left[k] + (right[k] - left[k]) * t;
        const float x0 = x + w * i / strips, x1 = x + w * (i + 1) / strips + (i + 1 < strips ? 0.01f : 0.0f);
        const float xy[8] = {X(x0 + lean) * kScale, Y(y) * kScale,     X(x1 + lean) * kScale, Y(y) * kScale,
                             X(x1) * kScale,        Y(y + h) * kScale, X(x0) * kScale,        Y(y + h) * kScale};
        r_->fillQuad(xy, c[0], c[1], c[2], c[3]);
    }
}

MenuPainter::Cached& MenuPainter::rasterise(const std::string& s, float size, bool perGlyphVerticalGradient, bool darkVerticalGradient, float letterSpacing) {
    const float density = std::max(1.0f, std::round(r_->pixelScale() * 4.0f) / 4.0f);
    // The fixed edge is one menu unit wide. Rasterise with that transparent border at the
    // actual pixel density, then combine the glyph alpha into a single antialiased outline.
    const int pad = cleanOutline_ && outline_ ? std::max(1, int(std::ceil(kScale * K() * density))) : 0;
    const std::string key = s + '\x01' + std::to_string(int(size * 10)) + '\x02' + std::to_string(pad) +
                            (darkVerticalGradient ? "\x03d" : perGlyphVerticalGradient ? "\x03g" : "\x03p") +
                            std::string("\x04") + std::to_string(int(std::lround(letterSpacing * 1000)));
    Cached& c = text_[key];
    const int generation = font_ ? font_->generation() : -2;
    if (c.tex.id && c.density == density && c.generation == generation && (pad == 0 || c.edgeTex.id)) return c;
    r_->destroy(c.tex);
    r_->destroy(c.edgeTex);
    c = Cached();
    if (!font_ || !font_->loaded() || s.empty()) return c;
    Image img;
    const float em = size * kScale * density;
    if (!font_->rasterise({s}, em, em * 1.2f, pad, img, perGlyphVerticalGradient, darkVerticalGradient,
                          letterSpacing * kScale * K() * density)) return c;

    c.tex = r_->upload(img);
    c.pad = float(pad) / density;
    c.w = float(img.width - 2 * pad) / density;
    c.h = float(img.height - 2 * pad) / density;
    c.density = density;
    c.generation = generation;
    if (pad > 0) {
        Image outline;
        if (makeGlyphOutline(img, pad, outline)) c.edgeTex = r_->upload(outline);
    }
    return c;
}

// Original mode: the game's eight copies, each one menu unit off in the opposite colour.
void MenuPainter::edge(const Texture& tex, float x, float y, float w, float h, float k, float alpha) {
    if (!outline_) return;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (dx || dy) r_->drawQuad(tex, X(x + float(dx)) * kScale, Y(y + float(dy)) * kScale, w, h, k, k, k, alpha);
}

void MenuPainter::text(const std::string& s, float x, float y, bool dark, float alpha, float size, bool verticalGradient, bool darkGradient, bool noOutline, float letterSpacing) {
    if (!r_ || s.empty() || alpha <= 0.001f) return;
    Cached& c = rasterise(s, menuSize(size) * K(), verticalGradient, darkGradient, letterSpacing);
    if (!c.tex.id) return;
    const float x0 = X(x) * kScale - c.pad, y0 = Y(y) * kScale - c.pad;
    const float w = c.w + 2 * c.pad, h = c.h + 2 * c.pad;
    const float k = dark && !darkGradient ? 0.0f : 1.0f;
    if (!noOutline) {
        if (outline_ && cleanOutline_ && c.edgeTex.id) {
            const float edge = dark ? 1.0f : 0.0f;
            r_->drawQuad(c.edgeTex, x0, y0, w, h, edge, edge, edge, alpha);
        } else if (!cleanOutline_) {
            edge(c.tex, x, y, c.w, c.h, dark ? 1.0f : 0.0f, alpha);
        }
    }
    // RGB in the cached texture is shaded independently per glyph; alpha remains the glyph mask.
    // Draw once with white tint so the per-character ramp is preserved and transparent pixels stay clear.
    r_->drawQuad(c.tex, x0, y0, w, h, k, k, k, alpha);
}

void MenuPainter::textTint(const std::string& s, float x, float y, float r, float g, float b, float alpha, float size,
                           bool verticalGradient) {
    if (!r_ || s.empty() || alpha <= 0.001f) return;
    Cached& c = rasterise(s, menuSize(size) * K(), verticalGradient);
    if (!c.tex.id) return;
    const float x0 = X(x) * kScale - c.pad, y0 = Y(y) * kScale - c.pad;
    const float w = c.w + 2 * c.pad, h = c.h + 2 * c.pad;
    const float edgeColor = r + g + b > 0.01f ? 0.0f : 1.0f;
    if (outline_ && cleanOutline_ && c.edgeTex.id)
        r_->drawQuad(c.edgeTex, x0, y0, w, h, edgeColor, edgeColor, edgeColor, alpha);
    else if (!cleanOutline_)
        edge(c.tex, x, y, c.w, c.h, edgeColor, alpha);
    r_->drawQuad(c.tex, x0, y0, w, h, r, g, b, alpha);
}

void MenuPainter::textCentred(const std::string& s, float x, float y, bool dark, float alpha, float size,
                                bool verticalGradient, bool darkGradient, bool noOutline, float letterSpacing) {
    if (!r_ || s.empty()) return;
    const Cached& c = rasterise(s, menuSize(size) * K(), verticalGradient, darkGradient, letterSpacing);
    text(s, x, y - c.h / kScale / K() / 2, dark, alpha, size, verticalGradient, darkGradient, noOutline, letterSpacing);
}

float MenuPainter::textWidth(const std::string& s, float size, float letterSpacing) {
    if (!r_ || s.empty()) return 0;
    return rasterise(s, menuSize(size) * K(), false, false, letterSpacing).w / kScale / K();
}

float MenuPainter::baselineFromCentre(float size) const {
    return font_ && font_->loaded() ? menuSize(size) * font_->baselineFromCentreEm() : 0.0f;
}

// Rebuild the window_hd section headings (World Select, Results, Camp, Shop, Options, Scenarios).
// The old pictures baked the words into a white strip fading to transparency at the right.
void MenuPainter::headingStrip(float x, float y, const std::string& label, float alpha, float width, float textSize, float textOffset) {
    if (!r_ || label.empty() || alpha <= 0.001f || width <= 20.0f) return;
    (void)textOffset; // sprite-specific baked-title offsets are replaced by baseline alignment.
    const float solid[4] = {1, 1, 1, alpha}, clear[4] = {1, 1, 1, 0};
    const float stripY = y + 16.0f;
    // window_hd's Camp sprite: a 2-unit vertical start mark, a 2-unit transparent gap,
    // 84 units of solid white, then the 116-unit fade to transparency (204 units total).
    quad(x, stripY, 2.0f, 16.0f, solid, solid);
    quad(x + 4.0f, stripY, std::min(84.0f, width - 4.0f), 16.0f, solid, solid);
    const float fadeWidth = std::max(0.0f, std::min(116.0f, width - 88.0f));
    if (fadeWidth > 0.0f) quad(x + 88.0f, stripY, fadeWidth, 16.0f, solid, clear);

    float size = textSize;
    const float maxTextWidth = std::max(8.0f, width - 18.0f);
    const float measured = textWidth(label, size);
    if (measured > maxTextWidth && measured > 0.0f)
        size = std::max(9.0f, std::floor(size * maxTextWidth / measured));
    // The baseline of the uppercase glyph sits on the strip's vertical centre line.
    textCentred(label, x + 9.0f, y + 24.0f - baselineFromCentre(size), true, alpha, size, false);
}

void MenuPainter::rowBar(float x, float y, const std::string& label, float alpha, bool highlight, float shade, float cap) {
    const float a = std::min(alpha, cap);
    // Keyboard cursor: the cyan the game lights pressed controls with (tex2d colour 0xEBFF4B, A B G R:
    // R 0x4B, G 0xFF, B 0xEB - Part 4 1174.45 s, the tapped Back bar)
    const float r = highlight ? 0x4B / 255.0f : shade, g = highlight ? 1 : shade, b = highlight ? 0xEB / 255.0f : shade;
    const float solid[4] = {r, g, b, a}, clear[4] = {r, g, b, 0};
    quad(x, y, 100, 24, solid, solid);
    quad(x + 100, y, 123, 24, solid, clear);
    textCentred(label, x + 8, y + 12, true, alpha, 14, false, true);
}

void MenuPainter::button(float x, float y, int word, const std::string& label, bool selected, float alpha, bool highlight,
                          bool verticalGradient) {
    // Base: left cap, 14 slices, right cap
    piece(x + 4, y, 0, 184, 24, 40, alpha);
    for (int k = 0; k < 14; ++k) piece(x + 28 + 4 * k, y, 28, 184, 4, 40, alpha);
    piece(x + 84, y, 28, 184, 24, 40, alpha);
    // Face: normal or selected strip
    const float r = highlight ? 0x4B / 255.0f : 1, b = highlight ? 0xEB / 255.0f : 1;
    piece(x, y + 4, 996, selected ? 932.0f : 900.0f, 16, 32, alpha, r, 1, b);
    for (int k = 0; k < 20; ++k) piece(x + 16 + 4 * k, y + 4, selected ? 120.0f : 76.0f, 188, 4, 32, alpha, r, 1, b);
    piece(x + 96, y + 4, selected ? 124.0f : 80.0f, 188, 16, 32, alpha, r, 1, b);
    if (word >= 0) {
        piece(x + 10, y + 8, float(word % 5) * 88, 512 + float(word / 5) * 24, 88, 24, alpha);
    } else if (!label.empty()) { // our labels: the game's font, centred on the face
        // (13, smaller to fit the face's 84: a long value - a fan translation's name - inside its button, #19)
        const float wide = textWidth(label, 13), size = wide > 84 ? std::max(8.0f, std::floor(13.0f * 84 / wide)) : 13.0f;
        // The original's words stay white with a black outline, also on a selected cyan face.
        textCentred(label, x + 56 - textWidth(label, size) / 2, y + 20, false, alpha, size, verticalGradient);
    }
}

void MenuPainter::plate(float x, float y, int icon, int word, bool selected, float alpha, bool dimmed,
                        const std::string& text, bool gradientText) {
    const float a = dimmed ? alpha * 0.45f : alpha;
    piece(x, y, 0, 184, 24, 40, a);
    for (int k = 0; k < 21; ++k) piece(x + 24 + 4 * k, y, 28, 184, 4, 40, a);
    piece(x + 108, y, 28, 184, 24, 40, a);
    piece(x + 28, y, selected ? 96.0f : 52.0f, 184, 24, 40, a);
    for (int k = 0; k < 17; ++k) piece(x + 52 + 4 * k, y, selected ? 120.0f : 76.0f, 184, 4, 40, a);
    piece(x + 120, y, selected ? 124.0f : 80.0f, 184, 16, 40, a);
    if (icon >= 0) piece(x + 8, y + 4, float(icon % 16) * 32, 376 + float(icon / 16) * 32, 32, 32, a);
    if (word >= 0) piece(x + 40, y + 8, float(word % 5) * 88, 512 + float(word / 5) * 24, 88, 24, a);
    else if (!text.empty()) {
        float size = 15;
        float wide = textWidth(text, size);
        if (gradientText && wide > 88) size = std::max(8.0f, std::floor(size * 88 / wide));
        wide = textWidth(text, size);
        // Align the capital glyph's midpoint with the light/dark seam across the oval face.
        textCentred(text, x + 40 + (88 - wide) / 2, y + 17, false, a, size, gradientText);
    }
}

void MenuPainter::gauge(float x, float y, int value, float alpha, bool active, float fillTo) {
    value = std::clamp(value, 0, 100);
    // ComUty_SideGuageDraw (Omega's sheet): the left cap 0x68 at x, the track 0x6a 24 times from
    // x + 20, 8 apart, the right cap 0x69 at x + 212, "Min" (0x6d) at (x - 1, y - 13), "Max" (0x6c) at
    // (x + 193, y - 13), the knob 0x6b at y - 16. (CR1's numbers had drawn the "Options" title (103)
    // as the left cap.)
    picture(104, x, y, alpha);
    for (int k = 0; k < 24; ++k) picture(106, x + 20 + 8 * k, y, alpha);
    picture(105, x + 212, y, alpha);
    picture(109, x - 1, y - 13, alpha);
    picture(108, x + 193, y - 13, alpha);
    // The fill (0xFFFF0000: tex2d's A B G R - blue, as Omega's Part 8 4927.3 s shows) up to the knob at 235 + value x 2.03 (SideGuageInit; menus
    // place gauges at x 224, so it is x + 11 + ...)
    // The knob jumps to the value; the fill eases there (SideGuageDraw: 10 frames from the old knob, #17). The
    // Vita's (_ComUty_SideGuageDraw's fourth argument, OptionPut's cursor gauge) fills light cyan, 0xffffff80
    const float knob = x + 11 + std::floor(value * 2.03f);
    const float fill = (fillTo < 0 ? knob : x + 11 + std::floor(std::clamp(fillTo, 0.0f, 100.0f) * 2.03f)) - (x + 14);
    const float colour[4] = {active ? 0x80 / 255.0f : 0, active ? 1.0f : 0, 1, alpha};
    if (fill > 0) {
        quad(x + 13, y + 6, 1, 2, colour, colour);
        quad(x + 14, y + 5, fill, 4, colour, colour);
    }
    const short* p = kMenuPicture[107]; // (the knob as it is: no build lights it - 3.1.0 49482, the Vita's 76193)
    piece(knob, y - 16, p[0], p[1], p[2], p[3], alpha);
}

void MenuPainter::arrowTriangle(float x, float y, float w, float h, bool pointsRight, float alpha) {
    if (!r_ || alpha <= 0.001f) return;
    const float tipX = pointsRight ? x + w : x;
    const float baseX = pointsRight ? x : x + w;
    const float tipY = y + h * 0.5f;
    const float xy[8] = pointsRight
        ? (float[8]){X(tipX) * kScale, Y(tipY) * kScale,
                     X(baseX) * kScale, Y(y + h) * kScale,
                     X(baseX) * kScale, Y(y) * kScale,
                     X(tipX) * kScale, Y(tipY) * kScale}
        : (float[8]){X(tipX) * kScale, Y(tipY) * kScale,
                     X(baseX) * kScale, Y(y) * kScale,
                     X(baseX) * kScale, Y(y + h) * kScale,
                     X(tipX) * kScale, Y(tipY) * kScale};
    // A solid black triangle, built by the renderer rather than sampled from window_hd.
    r_->fillQuad(xy, 0, 0, 0, alpha);
}

void MenuPainter::returnGlyph(float x, float y, float w, float h, float alpha) {
    if (!r_ || alpha <= 0.001f) return;

    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float outer = std::min(w * 0.48f, h * 0.42f);
    const float inner = outer * 0.53f;
    constexpr float pi = 3.14159265358979323846f;
    constexpr float start = 42.0f * pi / 180.0f;
    constexpr float finish = 300.0f * pi / 180.0f;
    // Tessellate more finely as the output resolution increases. This keeps the curved mark
    // smooth at 4K and high-DPI scales without ever sampling the low-resolution menu atlas.
    const int segments = std::clamp(int(std::ceil(18.0f * std::max(1.0f, r_->pixelScale()))), 18, 64);

    auto point = [&](float radius, float angle, float& px, float& py) {
        px = cx + std::cos(angle) * radius;
        py = cy + std::sin(angle) * radius;
    };
    auto fill = [&](float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
        const float xy[8] = {
            X(x0) * kScale, Y(y0) * kScale,
            X(x1) * kScale, Y(y1) * kScale,
            X(x2) * kScale, Y(y2) * kScale,
            X(x3) * kScale, Y(y3) * kScale,
        };
        r_->fillQuad(xy, 0, 0, 0, alpha);
    };

    for (int i = 0; i < segments; ++i) {
        const float a0 = start + (finish - start) * float(i) / float(segments);
        const float a1 = start + (finish - start) * float(i + 1) / float(segments);
        float ox0, oy0, ox1, oy1, ix0, iy0, ix1, iy1;
        point(outer, a0, ox0, oy0);
        point(outer, a1, ox1, oy1);
        point(inner, a1, ix1, iy1);
        point(inner, a0, ix0, iy0);
        fill(ox0, oy0, ox1, oy1, ix1, iy1, ix0, iy0);
    }

    // Arrowhead attached to the upper-right end of the circular stroke.
    float baseOuterX, baseOuterY, baseInnerX, baseInnerY;
    point(outer, finish, baseOuterX, baseOuterY);
    point(inner, finish, baseInnerX, baseInnerY);
    const float tipX = cx + std::cos(315.0f * pi / 180.0f) * outer * 1.45f;
    const float tipY = cy + std::sin(315.0f * pi / 180.0f) * outer * 1.45f;
    fill(baseOuterX, baseOuterY, tipX, tipY, baseInnerX, baseInnerY, tipX, tipY);
}

void MenuPainter::backButton(float x, float y, int pressed, int bob, float alpha, const std::string& label) {
    const float cyan[4] = {0x4B / 255.0f, 1, 0xEB / 255.0f, alpha}; // (tex2d's 0xEBFF4B is A B G R: cyan)
    const float white[4] = {1, 1, 1, alpha};
    // ComUty_CmdBack mode 2: the left arrow's box (24..51, faded in over 0..24), Back (56..176), the
    // right arrow's box (181..208, fading out to 232); the arrows bob inward (0..5). The part held
    // tints cyan - 1 Back, 2 the right arrow's box, 3 the left's (with their fades)
    const float* c = pressed == 1 ? cyan : white;
    const float* l = pressed == 3 ? cyan : white;
    const float* r = pressed == 2 ? cyan : white;
    const float clearL[4] = {l[0], l[1], l[2], 0}, clearR[4] = {r[0], r[1], r[2], 0};
    quad(x, y, 24, 24, clearL, l);
    quad(x + 24, y, 27, 24, l, l);
    quad(x + 56, y, 120, 24, c, c);
    quad(x + 181, y, 27, 24, r, r);
    quad(x + 208, y, 24, 24, r, clearR);
    // Arrows are vector triangles, so their edges are resolved at the current output scale.
    arrowTriangle(x + 24 + bob, y + 4, 16, 16, false, alpha);
    arrowTriangle(x + 197 - bob, y + 4, 16, 16, true, alpha);

    // The center face, curved return mark and lettering are all drawn at the active render scale.
    constexpr float glyphWidth = 18.0f, gap = 5.0f, labelMaxWidth = 92.0f;
    float size = 14.0f;
    float width = textWidth(label, size);
    if (width > labelMaxWidth) size = std::max(8.0f, std::floor(size * labelMaxWidth / width));
    width = textWidth(label, size);
    const float total = glyphWidth + (label.empty() ? 0.0f : gap) + width;
    const float start = x + 56.0f + (120.0f - total) / 2.0f;
    returnGlyph(start, y, glyphWidth, 24.0f, alpha);

    if (!label.empty()) {
        // Match the original button's plain black lettering; no menu outline or gradient.
        const bool savedOutline = outline_;
        const bool savedCleanOutline = cleanOutline_;
        outline_ = false;
        cleanOutline_ = true;
        textCentred(label, start + glyphWidth + gap, y + 12, true, alpha, size, false);
        outline_ = savedOutline;
        cleanOutline_ = savedCleanOutline;
    }
}

void MenuPainter::backBar(float x, float y, bool pressed, float alpha, const std::string& label) {
    const float cyan[4] = {0x4B / 255.0f, 1, 0xEB / 255.0f, alpha}; // (tex2d's 0xEBFF4B is A B G R: cyan)
    const float white[4] = {1, 1, 1, alpha};
    const float* c = pressed ? cyan : white;
    const float clear[4] = {c[0], c[1], c[2], 0};
    quad(x, y, 24, 24, clear, c);
    quad(x + 24, y, 184, 24, c, c);
    quad(x + 208, y, 24, 24, c, clear);

    const float arrowWidth = 20.0f, gap = 5.0f, maxLabelWidth = 76.0f;
    float size = 14.0f;
    float labelWidth = textWidth(label, size);
    if (labelWidth > maxLabelWidth)
        size = std::max(10.0f, std::floor(size * maxLabelWidth / labelWidth));
    labelWidth = textWidth(label, size);
    const float totalWidth = arrowWidth + (label.empty() ? 0.0f : gap) + labelWidth;
    const float ax = x + (232.0f - totalWidth) * 0.5f;
    returnGlyph(ax, y, arrowWidth, 24.0f, alpha);
    if (!label.empty()) {
        const bool savedOutline = outline_, savedCleanOutline = cleanOutline_;
        outline_ = false;
        cleanOutline_ = true;
        textCentred(label, ax + arrowWidth + gap, y + 12, true, alpha, size, false);
        outline_ = savedOutline;
        cleanOutline_ = savedCleanOutline;
    }
}

void MenuPainter::backOkBar(float x, float y, int pressed, float alpha, const std::string& label) {
    const float cyan[4] = {0x4B / 255.0f, 1, 0xEB / 255.0f, alpha}; // (tex2d's 0xEBFF4B is A B G R: cyan)
    const float white[4] = {1, 1, 1, alpha};
    const float* b = pressed == 1 ? cyan : white;
    const float* o = pressed == 5 ? cyan : white;
    const float clearB[4] = {b[0], b[1], b[2], 0}, clearO[4] = {o[0], o[1], o[2], 0};
    quad(x, y, 24, 24, clearB, b);
    quad(x + 24, y, 90, 24, b, b);
    quad(x + 118, y, 90, 24, o, o);
    quad(x + 208, y, 24, 24, o, clearO);
    constexpr float arrowWidth = 18.0f, gap = 5.0f, maxLabelWidth = 54.0f;
    float size = 14.0f;
    float labelWidth = textWidth(label, size);
    if (labelWidth > maxLabelWidth)
        size = std::max(9.0f, std::floor(size * maxLabelWidth / labelWidth));
    labelWidth = textWidth(label, size);
    const float total = arrowWidth + gap + labelWidth;
    const float start = x + 24.0f + (90.0f - total) * 0.5f;
    returnGlyph(start, y, arrowWidth, 24.0f, alpha);
    const bool savedOutline = outline_, savedCleanOutline = cleanOutline_;
    outline_ = false;
    cleanOutline_ = true;
    textCentred(label, start + arrowWidth + gap, y + 12, true, alpha, size, false);
    textCentred("OK", x + 126.0f + (64.0f - textWidth("OK", 14.0f)) * 0.5f, y + 12,
                true, alpha, 14.0f, false);
    outline_ = savedOutline;
    cleanOutline_ = savedCleanOutline;
}

void MenuPainter::digit(int d, float x, float y, float alpha) {
    if (d < 0) picture(66, x, y, alpha); // "/"
    else piece(x, y, float(d % 10) * 14, 0, 14, 14, alpha);
}

} // namespace cr
