#include "RibbonSource.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <execution>
#include <format>
#include <numeric>
#include <utility>
#include <vector>

namespace rock
{
namespace
{
template <typename Fn>
inline void ParallelForRows(int n, Fn&& fn)
{
    std::vector<int> rows(static_cast<size_t>(n));
    std::iota(rows.begin(), rows.end(), 0);
    std::for_each(std::execution::par, rows.begin(), rows.end(), std::forward<Fn>(fn));
}

// 決定論的な整数ハッシュ → [0, 1)。シードと格子座標だけで決まる。
inline float HashToUnitFloat(int ix, int iy, uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(ix) * 0x8da6b343u;
    h ^= static_cast<uint32_t>(iy) * 0xd8163841u;
    h ^= seed * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

inline float SmoothStep01(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// 値ノイズ 1 オクターブ。座標はワールドメートル / 波長。
float ValueNoise(float x, float y, uint32_t seed)
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const int ix = static_cast<int>(fx);
    const int iy = static_cast<int>(fy);
    const float tx = SmoothStep01(x - fx);
    const float ty = SmoothStep01(y - fy);
    const float v00 = HashToUnitFloat(ix, iy, seed);
    const float v10 = HashToUnitFloat(ix + 1, iy, seed);
    const float v01 = HashToUnitFloat(ix, iy + 1, seed);
    const float v11 = HashToUnitFloat(ix + 1, iy + 1, seed);
    const float top = std::lerp(v00, v10, tx);
    const float bottom = std::lerp(v01, v11, tx);
    return std::lerp(top, bottom, ty) * 2.0f - 1.0f; // [-1, 1]
}

float FbmNoise(float x, float y, int octaves, uint32_t seed)
{
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float norm = 0.0f;
    for (int octave = 0; octave < octaves; ++octave)
    {
        sum += amplitude * ValueNoise(x * frequency, y * frequency, seed + static_cast<uint32_t>(octave) * 101u);
        norm += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}
} // namespace

HeightfieldGrid BuildHeightfieldFromRibbon(const RibbonSettings& settings, int resolution, std::string* message)
{
    HeightfieldGrid grid;
    grid.resolution = std::clamp(resolution, 2, 2048);
    const int n = grid.resolution;

    // ワールド比例UV: 1テクセル = texelSizeCentimeters をグリッド全域で固定する。
    const float texelMeters = std::clamp(settings.texelSizeCentimeters, 0.5f, 100.0f) * 0.01f;
    grid.terrainSizeMeters = static_cast<float>(n) * texelMeters;

    const float roadHalf = std::max(0.1f, settings.roadHalfWidthMeters);
    const float shoulderWidth = std::max(0.0f, settings.shoulderWidthMeters);
    const float slopeWidth = std::max(0.0f, settings.slopeWidthMeters);
    const float crossfall = settings.crossfallPercent * 0.01f;
    const float shoulderCrossfall = settings.shoulderCrossfallPercent * 0.01f;
    const float slopeGrade = std::max(0.0f, settings.slopeGradePercent) * 0.01f;
    const float longGrade = settings.longitudinalGradePercent * 0.01f;
    const float noiseAmplitude = std::max(0.0f, settings.noiseAmplitudeMeters);
    const float noiseWavelength = std::max(0.25f, settings.noiseWavelengthMeters);
    const int noiseOctaves = std::clamp(settings.noiseOctaves, 1, 8);
    const uint32_t noiseSeed = static_cast<uint32_t>(settings.noiseSeed) * 2654435761u + 0x9e3779b9u;

    const size_t cellCount = static_cast<size_t>(n) * static_cast<size_t>(n);
    grid.heights.assign(cellCount, 0.0f);
    grid.mask.assign(cellCount, 0.0f);
    grid.deposits.assign(cellCount, 0.0f);
    grid.flows.assign(cellCount, 0.0f);
    grid.age.assign(cellCount, 0.0f);
    grid.baseZ.assign(cellCount, 0.0f);
    grid.normalZ.assign(cellCount, 1.0f);

    // 横断プロファイル: センターライン基準の P_z オフセット (縦断勾配を除く)。
    // v は横方向オフセット (m)、+v 側が片勾配の下り側。
    const auto profileZ = [&](float v) -> float {
        const float a = std::abs(v);
        if (a <= roadHalf)
        {
            return -crossfall * v; // 道路面 (単一平面の片勾配)
        }
        const float edgeZ = -crossfall * (v > 0.0f ? roadHalf : -roadHalf);
        if (a <= roadHalf + shoulderWidth)
        {
            return edgeZ - shoulderCrossfall * (a - roadHalf); // 路肩 (外向き下り)
        }
        const float shoulderEdgeZ = edgeZ - shoulderCrossfall * shoulderWidth;
        if (a <= roadHalf + shoulderWidth + slopeWidth)
        {
            return shoulderEdgeZ - slopeGrade * (a - roadHalf - shoulderWidth); // 法面
        }
        return shoulderEdgeZ - slopeGrade * slopeWidth; // 法尻から先は平坦に継続
    };

    // 初期変位ノイズの適用ウェイト。道路面は 0、路肩で立ち上がり法面で 1。
    const auto noiseZoneWeight = [&](float v) -> float {
        if (settings.noiseOnRoad)
        {
            return 1.0f;
        }
        const float a = std::abs(v);
        if (a <= roadHalf)
        {
            return 0.0f;
        }
        if (shoulderWidth <= 0.0f)
        {
            return 1.0f;
        }
        return SmoothStep01(std::clamp((a - roadHalf) / shoulderWidth, 0.0f, 1.0f));
    };

    // P_z ベイク (φ の組み立ての前に N_z を差分で取るため 2 パス)。
    ParallelForRows(n, [&](int y) {
        const float v = (static_cast<float>(y) - static_cast<float>(n - 1) * 0.5f) * texelMeters;
        const float zProfile = profileZ(v);
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(n);
        for (int x = 0; x < n; ++x)
        {
            const float s = static_cast<float>(x) * texelMeters; // 弧長 (直線センターラインなので u と一致)
            grid.baseZ[row + static_cast<size_t>(x)] = settings.baseElevationMeters - longGrade * s + zProfile;
        }
    });

    // N_z: ベースサーフェス勾配の中心差分から復元。リボンは F = 0 なので
    // ワールド勾配は (∂z/∂u / s_u, ∂z/∂v / s_v)。直線デモでは s_u = s_v = 1。
    ParallelForRows(n, [&](int y) {
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(n);
        for (int x = 0; x < n; ++x)
        {
            const int xm = std::max(0, x - 1);
            const int xp = std::min(n - 1, x + 1);
            const int ym = std::max(0, y - 1);
            const int yp = std::min(n - 1, y + 1);
            const float gx = (grid.baseZ[row + static_cast<size_t>(xp)] - grid.baseZ[row + static_cast<size_t>(xm)]) /
                (static_cast<float>(xp - xm) * texelMeters);
            const float gy = (grid.baseZ[static_cast<size_t>(yp) * static_cast<size_t>(n) + static_cast<size_t>(x)] -
                              grid.baseZ[static_cast<size_t>(ym) * static_cast<size_t>(n) + static_cast<size_t>(x)]) /
                (static_cast<float>(yp - ym) * texelMeters);
            grid.normalZ[row + static_cast<size_t>(x)] = 1.0f / std::sqrt(1.0f + gx * gx + gy * gy);
        }
    });

    // φ = P_z + h・N_z。h は fBm ノイズの山 (路肩・法面ゾーン)。
    // mask には初期 h のゾーンウェイトを入れてデバッグ可視化に使う。
    ParallelForRows(n, [&](int y) {
        const float v = (static_cast<float>(y) - static_cast<float>(n - 1) * 0.5f) * texelMeters;
        const float zoneWeight = noiseZoneWeight(v);
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(n);
        for (int x = 0; x < n; ++x)
        {
            const size_t idx = row + static_cast<size_t>(x);
            const float s = static_cast<float>(x) * texelMeters;
            float h = 0.0f;
            if (noiseAmplitude > 0.0f && zoneWeight > 0.0f)
            {
                const float noise = FbmNoise(s / noiseWavelength, v / noiseWavelength, noiseOctaves, noiseSeed);
                h = noiseAmplitude * zoneWeight * noise;
            }
            grid.heights[idx] = grid.baseZ[idx] + h * grid.normalZ[idx];
            grid.mask[idx] = zoneWeight;
        }
    });

    if (message != nullptr)
    {
        *message = std::format(
            "Ribbon {}x{} ({:.0f}cm/texel, {:.1f}m x {:.1f}m)",
            n, n, texelMeters * 100.0f, grid.terrainSizeMeters, grid.terrainSizeMeters);
    }
    return grid;
}
} // namespace rock
