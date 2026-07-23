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

inline float TexelMeters(const RibbonSettings& settings)
{
    return std::clamp(settings.texelSizeCentimeters, 0.5f, 100.0f) * 0.01f;
}

struct ChainPoint
{
    float x = 0.0f;
    float z = 0.0f;
    float height = 0.0f;
    PathSegmentType segmentToNext = PathSegmentType::Line;
};

// Path のエッジを端点から辿って一本のチェーンに並べる。エッジが無い /
// 分岐やループで辿れない場合は points の配列順にフォールバックする。
std::vector<ChainPoint> BuildPathChain(const PathSettings& path)
{
    std::vector<ChainPoint> chain;
    const auto findPoint = [&](GraphId id) -> const PathPoint* {
        const auto it = std::ranges::find_if(path.points, [id](const PathPoint& p) { return p.id == id; });
        return it != path.points.end() ? &*it : nullptr;
    };

    std::vector<const PathEdge*> edges;
    for (const PathEdge& edge : path.edges)
    {
        if (edge.enabled && findPoint(edge.fromPoint) != nullptr && findPoint(edge.toPoint) != nullptr)
        {
            edges.push_back(&edge);
        }
    }

    const auto pushPoint = [&](const PathPoint& p, PathSegmentType segment) {
        chain.push_back({p.x, p.z, p.height, segment});
    };

    if (!edges.empty())
    {
        // 次数 1 の点 (端点) を探す。無ければ (ループ等) 最初のエッジの始点から。
        const auto degree = [&](GraphId pointId) {
            int count = 0;
            for (const PathEdge* edge : edges)
            {
                if (edge->fromPoint == pointId || edge->toPoint == pointId) { ++count; }
            }
            return count;
        };
        GraphId startId = edges.front()->fromPoint;
        for (const PathEdge* edge : edges)
        {
            if (degree(edge->fromPoint) == 1) { startId = edge->fromPoint; break; }
            if (degree(edge->toPoint) == 1) { startId = edge->toPoint; break; }
        }

        std::vector<const PathEdge*> remaining = edges;
        GraphId currentId = startId;
        while (!remaining.empty())
        {
            const auto it = std::ranges::find_if(remaining, [&](const PathEdge* edge) {
                return edge->fromPoint == currentId || edge->toPoint == currentId;
            });
            if (it == remaining.end())
            {
                break;
            }
            const PathEdge* edge = *it;
            const GraphId nextId = edge->fromPoint == currentId ? edge->toPoint : edge->fromPoint;
            if (const PathPoint* current = findPoint(currentId))
            {
                pushPoint(*current, edge->segmentType);
            }
            currentId = nextId;
            remaining.erase(it);
            if (remaining.empty())
            {
                if (const PathPoint* last = findPoint(currentId))
                {
                    pushPoint(*last, edge->segmentType);
                }
            }
        }
    }

    if (chain.size() < 2)
    {
        chain.clear();
        for (const PathPoint& p : path.points)
        {
            pushPoint(p, path.defaultSegmentType);
        }
    }
    return chain;
}

struct DenseSample
{
    float x = 0.0f;
    float z = 0.0f;
    float height = 0.0f;
    float arcLength = 0.0f;
};

inline float CatmullRom(float p0, float p1, float p2, float p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// チェーンを細かく折れ線化して水平弧長を積算する。高さは各セグメント内で
// 弧長比の線形補間 (センターラインの縦断は緩いので十分)。
std::vector<DenseSample> DensifyChain(const std::vector<ChainPoint>& chain, float stepMeters)
{
    std::vector<DenseSample> samples;
    if (chain.size() < 2)
    {
        return samples;
    }
    const auto at = [&](int index) -> const ChainPoint& {
        return chain[static_cast<size_t>(std::clamp(index, 0, static_cast<int>(chain.size()) - 1))];
    };

    samples.push_back({chain[0].x, chain[0].z, chain[0].height, 0.0f});
    for (int seg = 0; seg + 1 < static_cast<int>(chain.size()); ++seg)
    {
        const ChainPoint& p1 = at(seg);
        const ChainPoint& p2 = at(seg + 1);
        const float segmentLength = std::hypot(p2.x - p1.x, p2.z - p1.z);
        const int steps = std::clamp(static_cast<int>(std::ceil(segmentLength / std::max(0.05f, stepMeters))), 1, 4096);
        for (int i = 1; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            float x = std::lerp(p1.x, p2.x, t);
            float z = std::lerp(p1.z, p2.z, t);
            if (p1.segmentToNext == PathSegmentType::CatmullRom)
            {
                const ChainPoint& p0 = at(seg - 1);
                const ChainPoint& p3 = at(seg + 2);
                x = CatmullRom(p0.x, p1.x, p2.x, p3.x, t);
                z = CatmullRom(p0.z, p1.z, p2.z, p3.z, t);
            }
            const DenseSample& prev = samples.back();
            const float ds = std::hypot(x - prev.x, z - prev.z);
            samples.push_back({x, z, std::lerp(p1.height, p2.height, t), prev.arcLength + ds});
        }
    }
    return samples;
}
} // namespace

RibbonCenterline BuildRibbonCenterline(const RibbonSettings& settings, const PathSettings* path, int resolution)
{
    const int n = std::clamp(resolution, 2, 2048);
    const float texel = TexelMeters(settings);
    const float longGrade = settings.longitudinalGradePercent * 0.01f;

    RibbonCenterline centerline;
    centerline.posX.resize(static_cast<size_t>(n));
    centerline.posZ.resize(static_cast<size_t>(n));
    centerline.elevation.resize(static_cast<size_t>(n));
    centerline.tanX.resize(static_cast<size_t>(n));
    centerline.tanZ.resize(static_cast<size_t>(n));
    centerline.curvature.assign(static_cast<size_t>(n), 0.0f);

    std::vector<DenseSample> dense;
    bool usePathHeights = false;
    if (path != nullptr)
    {
        const std::vector<ChainPoint> chain = BuildPathChain(*path);
        dense = DensifyChain(chain, texel * 0.5f);
        // Absolute 高さの点がひとつでもあれば Path の高さを縦断として使う。
        // それ以外 (ProjectToTerrain 等) は settings の縦断勾配にフォールバック。
        usePathHeights = std::ranges::any_of(path->points, [](const PathPoint& p) {
            return p.heightMode == PathPointHeightMode::Absolute;
        });
    }

    if (dense.size() >= 2)
    {
        centerline.fromPath = true;
        centerline.totalLengthMeters = dense.back().arcLength;
        size_t cursor = 0;
        for (int x = 0; x < n; ++x)
        {
            const float s = std::min(static_cast<float>(x) * texel, centerline.totalLengthMeters);
            while (cursor + 1 < dense.size() && dense[cursor + 1].arcLength < s)
            {
                ++cursor;
            }
            const DenseSample& a = dense[cursor];
            const DenseSample& b = dense[std::min(cursor + 1, dense.size() - 1)];
            const float span = std::max(b.arcLength - a.arcLength, 1e-6f);
            const float t = std::clamp((s - a.arcLength) / span, 0.0f, 1.0f);
            centerline.posX[static_cast<size_t>(x)] = std::lerp(a.x, b.x, t);
            centerline.posZ[static_cast<size_t>(x)] = std::lerp(a.z, b.z, t);
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            const float len = std::max(std::hypot(dx, dz), 1e-6f);
            centerline.tanX[static_cast<size_t>(x)] = dx / len;
            centerline.tanZ[static_cast<size_t>(x)] = dz / len;
            centerline.elevation[static_cast<size_t>(x)] = usePathHeights
                ? std::lerp(a.height, b.height, t)
                : settings.baseElevationMeters - longGrade * s;
        }
        // 符号付き水平曲率 κ = dT/ds・B (B = 左法線)。接線の中心差分から。
        for (int x = 0; x < n; ++x)
        {
            const int xm = std::max(0, x - 1);
            const int xp = std::min(n - 1, x + 1);
            const float ds = static_cast<float>(xp - xm) * texel;
            if (ds <= 0.0f)
            {
                continue;
            }
            const float dtx = (centerline.tanX[static_cast<size_t>(xp)] - centerline.tanX[static_cast<size_t>(xm)]) / ds;
            const float dtz = (centerline.tanZ[static_cast<size_t>(xp)] - centerline.tanZ[static_cast<size_t>(xm)]) / ds;
            const float bx = -centerline.tanZ[static_cast<size_t>(x)];
            const float bz = centerline.tanX[static_cast<size_t>(x)];
            centerline.curvature[static_cast<size_t>(x)] = dtx * bx + dtz * bz;
        }
    }
    else
    {
        // 直線デモセンターライン: x 軸方向、原点中心。
        centerline.totalLengthMeters = static_cast<float>(n) * texel;
        const float half = centerline.totalLengthMeters * 0.5f;
        for (int x = 0; x < n; ++x)
        {
            const float s = static_cast<float>(x) * texel;
            centerline.posX[static_cast<size_t>(x)] = s - half;
            centerline.posZ[static_cast<size_t>(x)] = 0.0f;
            centerline.tanX[static_cast<size_t>(x)] = 1.0f;
            centerline.tanZ[static_cast<size_t>(x)] = 0.0f;
            centerline.elevation[static_cast<size_t>(x)] = settings.baseElevationMeters - longGrade * s;
        }
    }
    return centerline;
}

HeightfieldGrid BuildHeightfieldFromRibbon(const RibbonSettings& settings, const PathSettings* path, int resolution, std::string* message)
{
    HeightfieldGrid grid;
    grid.resolution = std::clamp(resolution, 2, 2048);
    const int n = grid.resolution;

    // ワールド比例UV: 1テクセル = texelSizeCentimeters をグリッド全域で固定する。
    const float texelMeters = TexelMeters(settings);
    grid.terrainSizeMeters = static_cast<float>(n) * texelMeters;

    const float roadHalf = std::max(0.1f, settings.roadHalfWidthMeters);
    const float shoulderWidth = std::max(0.0f, settings.shoulderWidthMeters);
    const float slopeWidth = std::max(0.0f, settings.slopeWidthMeters);
    const float crossfall = settings.crossfallPercent * 0.01f;
    const float shoulderCrossfall = settings.shoulderCrossfallPercent * 0.01f;
    const float slopeGrade = std::max(0.0f, settings.slopeGradePercent) * 0.01f;
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

    const RibbonCenterline centerline = BuildRibbonCenterline(settings, path, n);

    // 横断プロファイル: センターライン基準の P_z オフセット (縦断を除く)。
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
            grid.baseZ[row + static_cast<size_t>(x)] = centerline.elevation[static_cast<size_t>(x)] + zProfile;
        }
    });

    // N_z: ベースサーフェス勾配の中心差分から復元。リボンは F = 0 なので
    // ワールド勾配は (∂z/∂u / s_u, ∂z/∂v / s_v)。s_u = 1 - κv (下限クランプ)、s_v = 1。
    ParallelForRows(n, [&](int y) {
        const float v = (static_cast<float>(y) - static_cast<float>(n - 1) * 0.5f) * texelMeters;
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(n);
        for (int x = 0; x < n; ++x)
        {
            const int xm = std::max(0, x - 1);
            const int xp = std::min(n - 1, x + 1);
            const int ym = std::max(0, y - 1);
            const int yp = std::min(n - 1, y + 1);
            const float su = std::max(1.0f - centerline.curvature[static_cast<size_t>(x)] * v, 0.05f);
            const float gx = (grid.baseZ[row + static_cast<size_t>(xp)] - grid.baseZ[row + static_cast<size_t>(xm)]) /
                (static_cast<float>(xp - xm) * texelMeters * su);
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
            "Ribbon {}x{} ({:.0f}cm/texel, {:.1f}m x {:.1f}m{})",
            n, n, texelMeters * 100.0f, grid.terrainSizeMeters, grid.terrainSizeMeters,
            centerline.fromPath ? std::format(", path {:.1f}m", centerline.totalLengthMeters) : std::string());
    }
    return grid;
}

MeshData BuildRibbonWorldMesh(const HeightfieldGrid& grid, const RibbonSettings& settings, const RibbonCenterline& centerline, int meshResolution, float uvGridSpacingMeters)
{
    MeshData mesh;
    const int n = grid.resolution;
    if (n < 2 || grid.heights.size() < static_cast<size_t>(n) * static_cast<size_t>(n) ||
        static_cast<int>(centerline.posX.size()) != n)
    {
        return mesh;
    }

    const float texel = TexelMeters(settings);
    const float atlasHalf = static_cast<float>(n) * texel * 0.5f;
    // 道路〜法尻 + 余白のバンドだけメッシュ化する (外側の平坦域はプレビュー不要)。
    const float bandHalf = std::min(
        std::max(0.1f, settings.roadHalfWidthMeters) + std::max(0.0f, settings.shoulderWidthMeters) +
            std::max(0.0f, settings.slopeWidthMeters) + 2.0f,
        atlasHalf);

    // u はセンターラインの実長まで、v はバンド幅まで。meshResolution 相当の
    // 頂点密度になるよう u / v 共通の stride で間引く。
    const int uCount = std::clamp(static_cast<int>(centerline.totalLengthMeters / texel) + 1, 2, n);
    const int stride = std::clamp(n / std::clamp(meshResolution, 16, 2048), 1, n);
    const int bandTexels = static_cast<int>(bandHalf / texel);

    std::vector<int> uSamples;
    for (int x = 0; x < uCount; x += stride)
    {
        uSamples.push_back(x);
    }
    if (uSamples.back() != uCount - 1)
    {
        uSamples.push_back(uCount - 1);
    }
    std::vector<int> vSamples;
    for (int offset = -bandTexels; offset <= bandTexels; offset += stride)
    {
        vSamples.push_back(offset);
    }
    if (vSamples.back() != bandTexels)
    {
        vSamples.push_back(bandTexels);
    }

    const int uNum = static_cast<int>(uSamples.size());
    const int vNum = static_cast<int>(vSamples.size());
    mesh.vertices.resize(static_cast<size_t>(uNum) * static_cast<size_t>(vNum));

    for (int vi = 0; vi < vNum; ++vi)
    {
        const float v = static_cast<float>(vSamples[static_cast<size_t>(vi)]) * texel;
        for (int ui = 0; ui < uNum; ++ui)
        {
            const int x = uSamples[static_cast<size_t>(ui)];
            const int y = std::clamp(vSamples[static_cast<size_t>(vi)] + (n - 1) / 2, 0, n - 1);
            const size_t gridIdx = static_cast<size_t>(y) * static_cast<size_t>(n) + static_cast<size_t>(x);

            // カーブ内側のカスプ回避: オフセットを曲率半径の手前でクランプする。
            const float kappa = centerline.curvature[static_cast<size_t>(x)];
            float vClamped = v;
            if (kappa > 1e-4f)
            {
                vClamped = std::min(v, 0.95f / kappa);
            }
            else if (kappa < -1e-4f)
            {
                vClamped = std::max(v, 0.95f / kappa);
            }

            const float bx = -centerline.tanZ[static_cast<size_t>(x)];
            const float bz = centerline.tanX[static_cast<size_t>(x)];
            MeshVertex& vertex = mesh.vertices[static_cast<size_t>(vi) * static_cast<size_t>(uNum) + static_cast<size_t>(ui)];
            vertex.x = centerline.posX[static_cast<size_t>(x)] + vClamped * bx;
            vertex.z = centerline.posZ[static_cast<size_t>(x)] + vClamped * bz;
            vertex.y = grid.heights[gridIdx]; // 標高 = φ (変位込みのワールドZ)
            vertex.mask = grid.mask[gridIdx];
        }
    }

    // 三角形と頂点法線 (面法線の加算平均)。
    mesh.triangles.reserve(static_cast<size_t>(uNum - 1) * static_cast<size_t>(vNum - 1) * 2u);
    for (int vi = 0; vi + 1 < vNum; ++vi)
    {
        for (int ui = 0; ui + 1 < uNum; ++ui)
        {
            const uint32_t i00 = static_cast<uint32_t>(vi * uNum + ui);
            const uint32_t i10 = i00 + 1u;
            const uint32_t i01 = i00 + static_cast<uint32_t>(uNum);
            const uint32_t i11 = i01 + 1u;
            mesh.triangles.push_back({i00, i01, i10});
            mesh.triangles.push_back({i10, i01, i11});
        }
    }
    for (const MeshTriangle& tri : mesh.triangles)
    {
        MeshVertex& a = mesh.vertices[tri.a];
        MeshVertex& b = mesh.vertices[tri.b];
        MeshVertex& c = mesh.vertices[tri.c];
        const float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
        const float acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;
        const float nx = aby * acz - abz * acy;
        const float ny = abz * acx - abx * acz;
        const float nz = abx * acy - aby * acx;
        a.nx += nx; a.ny += ny; a.nz += nz;
        b.nx += nx; b.ny += ny; b.nz += nz;
        c.nx += nx; c.ny += ny; c.nz += nz;
    }
    for (MeshVertex& vertex : mesh.vertices)
    {
        const float len = std::sqrt(vertex.nx * vertex.nx + vertex.ny * vertex.ny + vertex.nz * vertex.nz);
        if (len > 1e-12f)
        {
            vertex.nx /= len;
            vertex.ny /= len;
            vertex.nz /= len;
        }
        else
        {
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
        }
    }

    // ワイヤーフレーム用エッジ = UV確認の iso-u / iso-v 線。UV空間で
    // uvGridSpacingMeters ごとに引くので、ワールドではカーブ内側で詰まり
    // 外側で開く (= メトリック歪みの可視化)。
    const float spacing = std::max(uvGridSpacingMeters, texel * static_cast<float>(stride));
    const auto uIndexForMeters = [&](float meters) {
        return std::clamp(static_cast<int>(std::lround(meters / texel / static_cast<float>(stride))), 0, uNum - 1);
    };
    const auto vIndexForMeters = [&](float meters) {
        return std::clamp(static_cast<int>(std::lround((meters / texel + static_cast<float>(bandTexels)) / static_cast<float>(stride))), 0, vNum - 1);
    };
    // iso-v 線 (u 方向に走る線): v = 0, ±spacing, ±2·spacing, ...
    std::vector<int> isoVRows;
    for (float v = 0.0f; v <= bandHalf + 0.5f * spacing; v += spacing)
    {
        isoVRows.push_back(vIndexForMeters(v));
        if (v > 0.0f)
        {
            isoVRows.push_back(vIndexForMeters(-v));
        }
    }
    std::ranges::sort(isoVRows);
    isoVRows.erase(std::unique(isoVRows.begin(), isoVRows.end()), isoVRows.end());
    for (const int vi : isoVRows)
    {
        for (int ui = 0; ui + 1 < uNum; ++ui)
        {
            mesh.edges.push_back({static_cast<uint32_t>(vi * uNum + ui), static_cast<uint32_t>(vi * uNum + ui + 1)});
        }
    }
    // iso-u 線 (v 方向に走る線): u = 0, spacing, 2·spacing, ...
    std::vector<int> isoUCols;
    for (float u = 0.0f; u <= centerline.totalLengthMeters + 0.5f * spacing; u += spacing)
    {
        isoUCols.push_back(uIndexForMeters(u));
    }
    std::ranges::sort(isoUCols);
    isoUCols.erase(std::unique(isoUCols.begin(), isoUCols.end()), isoUCols.end());
    for (const int ui : isoUCols)
    {
        for (int vi = 0; vi + 1 < vNum; ++vi)
        {
            mesh.edges.push_back({static_cast<uint32_t>(vi * uNum + ui), static_cast<uint32_t>((vi + 1) * uNum + ui)});
        }
    }
    return mesh;
}
} // namespace rock
