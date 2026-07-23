#include "UvChecker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rock
{
namespace
{
// 3x5 ビットマップフォント (0-9, A-J)。各行 3 ビット、上の行が下位側。
constexpr uint16_t kGlyphs[20] = {
    // 0-9
    0b111'101'101'101'111,
    0b010'110'010'010'111,
    0b111'001'111'100'111,
    0b111'001'111'001'111,
    0b101'101'111'001'001,
    0b111'100'111'001'111,
    0b111'100'111'101'111,
    0b111'001'001'001'001,
    0b111'101'111'101'111,
    0b111'101'111'001'111,
    // A-J
    0b010'101'111'101'101,
    0b110'101'110'101'110,
    0b011'100'100'100'011,
    0b110'101'101'101'110,
    0b111'100'110'100'111,
    0b111'100'110'100'100,
    0b011'100'101'101'011,
    0b101'101'111'101'101,
    0b111'010'010'010'111,
    0b001'001'001'101'010,
};

inline bool GlyphPixel(int glyphIndex, int px, int py)
{
    if (glyphIndex < 0 || glyphIndex >= 20 || px < 0 || px >= 3 || py < 0 || py >= 5)
    {
        return false;
    }
    const int bit = (4 - py) * 3 + (2 - px);
    return ((kGlyphs[glyphIndex] >> bit) & 1u) != 0u;
}

inline uint32_t HashCell(int i, int j)
{
    uint32_t h = static_cast<uint32_t>(i) * 0x8da6b343u ^ static_cast<uint32_t>(j) * 0xd8163841u;
    h ^= h >> 13;
    h *= 0x7feb352du;
    h ^= h >> 16;
    return h;
}

constexpr float kPalette[8][3] = {
    {0.83f, 0.38f, 0.38f}, // 赤
    {0.38f, 0.62f, 0.84f}, // 青
    {0.46f, 0.74f, 0.42f}, // 緑
    {0.85f, 0.76f, 0.38f}, // 黄
    {0.70f, 0.48f, 0.80f}, // 紫
    {0.42f, 0.75f, 0.70f}, // 青緑
    {0.85f, 0.57f, 0.34f}, // 橙
    {0.58f, 0.58f, 0.62f}, // 灰
};
} // namespace

void UvCheckerColor(float uMeters, float vMeters, float cellMeters, float texelMeters, float& r, float& g, float& b)
{
    const float cell = std::max(cellMeters, texelMeters * 2.0f);
    const float ci = std::floor(uMeters / cell);
    const float cj = std::floor(vMeters / cell);
    const int i = static_cast<int>(ci);
    const int j = static_cast<int>(cj);
    const float fx = uMeters / cell - ci; // セル内 0..1
    const float fy = vMeters / cell - cj;

    const float* palette = kPalette[HashCell(i, j) & 7u];
    const float shade = (((i + j) & 1) == 0) ? 1.0f : 0.62f;
    r = palette[0] * shade;
    g = palette[1] * shade;
    b = palette[2] * shade;

    // 4x4 サブグリッド線とセル境界線
    const float lineWidth = std::clamp(texelMeters / cell * 1.2f, 0.004f, 0.08f);
    const float sx = fx * 4.0f - std::floor(fx * 4.0f);
    const float sy = fy * 4.0f - std::floor(fy * 4.0f);
    if (sx < lineWidth * 4.0f || sy < lineWidth * 4.0f)
    {
        r *= 0.82f; g *= 0.82f; b *= 0.82f;
    }
    if (fx < lineWidth || fy < lineWidth || fx > 1.0f - lineWidth || fy > 1.0f - lineWidth)
    {
        r *= 0.45f; g *= 0.45f; b *= 0.45f;
    }

    // セル中央のラベル「列数字 + 行英字」(例: 3B)。小さいセルでは省略。
    const float cellTexels = cell / texelMeters;
    if (cellTexels >= 12.0f)
    {
        const int scale = std::max(1, static_cast<int>(cellTexels / 12.0f));
        const int px = static_cast<int>(fx * cellTexels);
        const int py = static_cast<int>(fy * cellTexels);
        const int labelWidth = 7 * scale;  // 3 + 1 + 3
        const int labelHeight = 5 * scale;
        const int originX = (static_cast<int>(cellTexels) - labelWidth) / 2;
        const int originY = (static_cast<int>(cellTexels) - labelHeight) / 2;
        const int lx = px - originX;
        const int ly = py - originY;
        if (lx >= 0 && lx < labelWidth && ly >= 0 && ly < labelHeight)
        {
            const int digitGlyph = ((i % 10) + 10) % 10;
            const int letterGlyph = 10 + ((j % 10) + 10) % 10;
            // 2D ビューは v 軸を上下反転して表示するため、グリフは v 反転で
            // 焼いておくと画面上で正しい向きに読める。
            const int glyphRow = 4 - ly / scale;
            bool on = false;
            if (lx < 3 * scale)
            {
                on = GlyphPixel(digitGlyph, lx / scale, glyphRow);
            }
            else if (lx >= 4 * scale)
            {
                on = GlyphPixel(letterGlyph, (lx - 4 * scale) / scale, glyphRow);
            }
            if (on)
            {
                r = 0.08f; g = 0.08f; b = 0.08f;
            }
        }
    }
}
} // namespace rock
