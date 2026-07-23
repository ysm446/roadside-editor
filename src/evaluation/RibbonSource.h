#pragma once

#include <string>
#include <vector>

#include "../node_graph.h"

namespace rock
{
// UVグリッドの各 u 列に対応するセンターラインのサンプル。
// 水平面は (x, z)、標高は y (ビューポートの上方向) の座標系。
// B = 左法線 (-tanZ, tanX)。s_u = 1 - curvature・v (v は B 方向のオフセット)。
struct RibbonCenterline
{
    std::vector<float> posX;      // 水平位置 x
    std::vector<float> posZ;      // 水平位置 z
    std::vector<float> elevation; // センターライン標高 (縦断勾配 or Path 高さ)
    std::vector<float> tanX;      // 単位接線 (水平)
    std::vector<float> tanZ;
    std::vector<float> curvature; // 符号付き水平曲率 κ(u) [1/m]
    float totalLengthMeters = 0.0f;
    bool fromPath = false;
};

// センターラインを弧長パラメータで resolution 列にサンプリングする。
// path が nullptr または有効点が 2 未満なら直線デモセンターライン
// (x 軸方向、原点中心、縦断勾配は settings から)。
RibbonCenterline BuildRibbonCenterline(const RibbonSettings& settings, const PathSettings* path, int resolution);

// RibbonSettings のプロファイルをUVグリッドへ掃引ベイクする。
// heights = φ = P_z + h・N_z、baseZ = P_z、normalZ = N_z。
// grid.terrainSizeMeters は resolution x texelSizeCentimeters から決まり、
// グローバルの Terrain Size 設定は使用しない (ワールド比例UVの強制)。
HeightfieldGrid BuildHeightfieldFromRibbon(const RibbonSettings& settings, const PathSettings* path, int resolution, std::string* message);

// 侵食後のUVグリッドをワールド空間の掃引リボンメッシュへ復元する (3D プレビュー用)。
// 頂点位置: 水平 = C(s) + v・B、標高 = φ (= heights)。メッシュは道路〜法尻の
// バンドのみ生成し、カーブ内側のカスプ回避のため v は 1/κ 未満にクランプする。
// エッジは uvGridSpacingMeters 間隔の iso-u / iso-v 線 (UV確認用。ワイヤー
// フレーム表示で見える。UV等間隔なのでカーブ内側では詰まって見える)。
MeshData BuildRibbonWorldMesh(const HeightfieldGrid& grid, const RibbonSettings& settings, const RibbonCenterline& centerline, int meshResolution, float uvGridSpacingMeters);
} // namespace rock
